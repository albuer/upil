#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <upil_event/upil_event_wrapper.h>

typedef int (*MSG_PROCESSOR)(int fd, short events, void *userdata);

typedef struct MSG{
    // 指向下一个消息
    struct MSG *next;
    struct MSG *prev;

    int what;
    int data_size;
    void* data;
    int flags;
    uint when;
    struct HANDLER* handler;
    struct upil_event ev;
}MSG;

/*
 * HANDLER 是一个消息处理机，包含一个工作线程来处理发送给它的事件
 */
typedef struct HANDLER{
    // 待处理的消息队列
    MSG             msg_queue; 

    // 推迟的消息队列，主要用于状态机，该消息队列只有当状态发生转换时，才会出列
    MSG             defer_msg_queue; 

    // 消息处理回调函数，所有消息都将会调用该函数来分发处理
    MSG_PROCESSOR   msg_processor;

    // 记录该handler中未处理的消息个数
    int             msg_count;

    // 事件循环，送到该handler的消息将会转换为事件并送入事件循环中
    uew_context     loop;

    // 该handler的属主
    void*           owner;
}HANDLER;

MSG* obtain_message(HANDLER* handler, int what, void* data, int data_size);
void send_message_delay(MSG* msg, uint msec);
#define send_message(msg)   send_message_delay(msg, 0)
void remove_message(MSG* msg);
void remove_message_id(MSG* queue, int what);
void defer_message(MSG* msg);
void process_defer_msg(MSG* queue);
int start_handler(HANDLER* handler, MSG_PROCESSOR func);

#endif

