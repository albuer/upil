#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include <upil_config.h>
#include <upil_sm.h>
#include <upil.h>


#define LOG_TAG "UPIL_HIDDUMP"
#define LOG_NDEBUG 0 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

#define DEFAULT_DEV     "/dev/hidraw0"
#define HIDPDU_LINE_BYTES       8
#define HIDPUD_LINE_MAX         10

static struct upil_event s_device_event;
static uew_context event_loop;
static uchar s_pdu_buf[HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX] = "";
static int s_pdu_offset=0;

static void hexdump(const char *title, const uchar *data, size_t len)
{
    size_t i=0;
    char line[1024] = "";
    char sbyte[4];

    sprintf(line, "%s ", title);

    if (data==NULL || len<=0)
    {
        strcat(line, " [NULL]\n");

        fprintf(stdout, "%s", line);
        
        return;
    }
    
    for(;i<len;i++)
    {
        sprintf(sbyte, "%02X ", data[i]);
        strcat(line, sbyte);
    }
    strcat(line, "\n");

    fprintf(stdout, "%s", line);
}

static int recv_event_cb(int fd, short flags, void *param)
{
    int ret = 0;
    int r_bytes = 0;
    uchar buf[512];
    uchar* pbuf = buf;
//    TPPEvt_t event;

    r_bytes = read(fd, buf, 512);
//    fprintf(stdout, "readed bytes %d\n", r_bytes);

    if (r_bytes<0)
    {
        fprintf(stderr, "read device failure, must exit.\n");
        uew_event_del(&event_loop, (struct upil_event*)param, 0);
        close(fd);
        return -1;
    }
    
    while (r_bytes >= HIDPDU_LINE_BYTES)
    {
        hexdump("<<", pbuf, HIDPDU_LINE_BYTES);
        
        if (pbuf[0]==0x03 && (pbuf[1]&0x80)==0x80)
        {
            fprintf(stdout, "[ACK]\n");
            buf[1] &= 0x3F;
        }
        
        ret = hidpdu2event(pbuf, s_pdu_buf, &s_pdu_offset, NULL);

        if (ret) {
            if (ret>0)
            {
                hexdump("++", s_pdu_buf, s_pdu_offset);
            }
            
            memset(s_pdu_buf, 0, s_pdu_offset);
            s_pdu_offset = 0;
        }

        pbuf += HIDPDU_LINE_BYTES;
        r_bytes -= HIDPDU_LINE_BYTES;
    }
    if (r_bytes>0)
        hexdump("<<", pbuf, r_bytes);

    return 0;
}

void dump_file(const char* fname)
{
    FILE* fp = NULL;
    char line[1024] = "";

    fp = fopen(fname, "r");
    while (fgets(line, 1023, fp)) {
        
    }

    fclose(fp);
}

void usage(char *argv0)
{
    fprintf(stdout, "Usage %s:\n", argv0);
	fprintf(stdout, "\t<-d> to monitor for device\n");
	fprintf(stdout, "\t<-f> to dump file\n");
}

// hiddump hidraw0  
int main(int argc, char** argv)
{
    char file_name[512];
    int fd = -1;
    int c;
    int is_dev = 1;

    if (argc==1)
    {
        sprintf(file_name, "%s", DEFAULT_DEV);
        is_dev = 1;
    }

	while (argc>1) {
		c = getopt (argc, argv, "df");

		if (c == -1) {
			break;
		}

		switch (c) {
			case 'd':
                is_dev = 1;
                sprintf(file_name, "%s", optarg);
				break;
            case 'f':
                is_dev = 0;
                sprintf(file_name, "%s", optarg);
                break;
			case '?':
			default:
				usage(argv[0]);
                c = -1;
				break;
		}
	}
    if (c==-1) return -1;

    if (is_dev) {
        fprintf(stdout, "open device: %s\n", file_name);
        fd = open(file_name, O_RDWR);
        if (fd<0)
        {
            fprintf(stderr, "Can not open file: %s\n", strerror(errno));
            return -1;
        }
        
        if (uew_startEventLoop(&event_loop))
        {
            fprintf(stderr, "Failed in start event loop\n");
            exit(-1);
        }
        uew_event_set(&s_device_event, fd, 1, recv_event_cb, &s_device_event);
        uew_event_add_wakeup(&event_loop, &s_device_event);
        
        while (1);
    } else {
        dump_file(file_name);
    }
    
    close(fd);

    return 0;
}

