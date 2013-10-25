#include <upil_config.h>
#include <upil_ctrl.h>

#define LOG_TAG "UPIL_CLIENT"
#include <utils/Log.h>

#define MAX_COMMAND_BYTES   512
#define MAX_RESPONSE_BYTES  1024

static char cmd_buf[MAX_COMMAND_BYTES];
static char response_buf[MAX_RESPONSE_BYTES];

static void * readLoop(void *param) {
    char buf[512];
    
    while (upil_monitor_recv(buf, 512)>=0)
    {
        fprintf(stdout, "<< %s\n", buf);
    }

    fprintf(stdout, "exit read loop\n", buf);
    
    kill(0, SIGKILL);
    
    return NULL;
}

void print_help()
{
    fprintf(stdout, "-------- upil command --------\n");
    fprintf(stdout, "DIALxxxx\t\tcall number or connect\n");
    fprintf(stdout, "HANGUP\t\thangup\n");
}

/*
    DIALxxxx
    HANGUP
 */
int main()
{
    pthread_attr_t attr;
    pthread_t tid_dispatch;

    // Connect to upil ctrl
    if (upil_connect())
    {
        fprintf(stderr, "upil connect failed!\n");
        return -1;
    }
    
    pthread_attr_init(&attr);
    pthread_create(&tid_dispatch, &attr, readLoop, NULL);

    while( gets(cmd_buf) )
    {
//        fprintf(stdout, "cmd: %s\n", cmd_buf);
        
        if(!strcmp(cmd_buf, "EXIT"))
            break;
        else if(!strcmp(cmd_buf, "HELP"))
        {
            print_help();
            continue;
        }
        else if( cmd_buf[0] == '\0' )
            continue;

        upil_ctrl_request(cmd_buf, response_buf, MAX_RESPONSE_BYTES);
        if (response_buf[0])
            fprintf(stdout, ">> %s\n", response_buf);

        memset(cmd_buf, 0, MAX_COMMAND_BYTES);
    }

    // Disconnect from upil
    upil_disconnect();

    return 0;
}


