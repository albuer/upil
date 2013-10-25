#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <upil_event/upil_event_wrapper.h>

typedef int (*MSG_PROCESSOR)(int fd, short events, void *userdata);

typedef struct MSG{
    // ָ����һ����Ϣ
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
 * HANDLER ��һ����Ϣ�����������һ�������߳��������͸������¼�
 */
typedef struct HANDLER{
    // ���������Ϣ����
    MSG             msg_queue; 

    // �Ƴٵ���Ϣ���У���Ҫ����״̬��������Ϣ����ֻ�е�״̬����ת��ʱ���Ż����
    MSG             defer_msg_queue; 

    // ��Ϣ����ص�������������Ϣ��������øú������ַ�����
    MSG_PROCESSOR   msg_processor;

    // ��¼��handler��δ�������Ϣ����
    int             msg_count;

    // �¼�ѭ�����͵���handler����Ϣ����ת��Ϊ�¼��������¼�ѭ����
    uew_context     loop;

    // ��handler������
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

