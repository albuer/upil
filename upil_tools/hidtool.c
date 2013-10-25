#include <upil_config.h>
#include <unistd.h>
#include <stdlib.h>

#include <upil_sm.h>
#include <upil.h>


#define LOG_TAG "UPIL_HIDTOOL"
#define LOG_NDEBUG 1 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

#define DEFAULT_DEV     "/dev/hidraw0"

 
int str2hex(const char* str, uchar *data, int data_size)
{
    char *ptr = NULL;
    int i=0;

    for(;i<data_size; i++)
    {
        unsigned long n;
        n = strtoul(str, &ptr, 16);
        if (n>0xFF)
            break;
        if (ptr==str)
            break;
        data[i] = n;

        if (*ptr == '\0')
            break;
        str = ptr+1;
    }

    return i;
}

int main(int argc, char** argv)
{
    int fd = -1;
    uchar data[8];

    if (argc<2)
        LOGD("%s cmd", argv[0]);

    LOGV("open device: %s", DEFAULT_DEV);
    fd = open(DEFAULT_DEV, O_RDWR);
    if (fd<0)
    {
        upil_error("Can not open file: %s\n", strerror(errno));
        return -1;
    }

    str2hex(argv[1], data, 8);
    upil_dump("send:", data, 8);
    write(fd, data, 8);    

    close(fd);

    return 0;
}

