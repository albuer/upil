#include <upil_config.h>

#define LOG_TAG "UPIL_HANDLER_TEST"
#include <utils/Log.h>

int my_process_message(int fd, short flags, void *param)
{
    MSG* msg = (MSG*)param;
    ENTER_LOG();

    LOGV("Process msg(%p) event(%p) what(%d)", msg, &msg->ev, msg->what);

    // 消息从队列中移除
    remove_from_list(msg);

    LOGV("process message...\n");
    
    LOGV("free msg(%p)", msg);
    // 释放
    free(msg);

    LEAVE_LOG();

    return 0;
}


int main()
{
    HANDLER handler;
    MSG* msg;
    int i=0;
    if (start_handler(&handler, my_process_message)!=0)
    {
        LOGE("start handler failed!");
        return -1;
    }

    sleep(3);

    msg = obtain_message(&handler, 1, "TEST", strlen("TEST"));

    send_message_delay(msg, 4*1000);

    msg = obtain_message(&handler, 3, "TEST", strlen("TEST"));

    send_message_delay(msg, 5*1000);
/*    
    while(1) {
    for (i=0; i<9; i++) {
        LOGV("++ obtain message: %d\n", i);
        msg = obtain_message(&handler, i, "TEST", strlen("TEST"));
        if (msg) {
        LOGV("++ send_message_delay: %d\n", i);
        sleep(1);
        send_message_delay(msg, i*1000);
            }
    }
        }

    */    
    while(1);
    return 0;
}

