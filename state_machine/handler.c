#include <upil_config.h>
#include "handler.h"

#define LOG_TAG "UPIL_HANDLER"
//#define LOG_NDEBUG 1 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

#define MAX_MSG_QUEUE_SIZE       32

static void dump_msg(const char* title, MSG* msg)
{
	if (MSG_MSGDUMP < upil_debug_level)
		return;

    upil_printf(MSG_MSGDUMP, "%s\n", title==NULL?"":title);
    upil_printf(MSG_MSGDUMP, "msg: %p\n", msg);
    upil_printf(MSG_MSGDUMP, "msg next: %p\n", msg->next);
    upil_printf(MSG_MSGDUMP, "msg prev: %p\n", msg->prev);
    upil_printf(MSG_MSGDUMP, "what: %d\n", msg->what);
    upil_printf(MSG_MSGDUMP, "data size: %d\n", msg->data_size);
    upil_printf(MSG_MSGDUMP, "data: %p\n", msg->data);
    upil_printf(MSG_MSGDUMP, "handler: %p\n", msg->handler);
}

static void init_queue(MSG* queue)
{
    ENTER_LOG();
    
    memset(queue, 0, sizeof(MSG));
    queue->next = queue;
    queue->prev = queue;
    upil_printf(MSG_MSGDUMP, "init queue: %p", queue);
    LEAVE_LOG();
}

static void addToQueue(MSG* msg, MSG* queue)
{
    ENTER_LOG();
    upil_printf(MSG_MSGDUMP, "MSG(%p) add to QUEUE(%p)", msg, queue);
    msg->next = queue;
    msg->prev = queue->prev;
    msg->prev->next = msg;
    queue->prev = msg;
    LEAVE_LOG();
}

static void removeFromQueue(MSG* msg)
{
    ENTER_LOG();
    if (msg && msg->next && msg->prev)
    {
        msg->next->prev = msg->prev;
        msg->prev->next = msg->next;
        msg->next = NULL;
        msg->prev = NULL;
    }
    else
        upil_error("ERROR MSG: %p <- %p -> %p", 
                        msg?msg->prev:NULL,
                        msg,
                        msg?msg->next:NULL);
    LEAVE_LOG();
}

static MSG* obtain(HANDLER* handler)
{
    MSG* msg = NULL;
    ENTER_LOG();
    
    if ((handler->msg_count+1)<MAX_MSG_QUEUE_SIZE) {
        msg = malloc(sizeof(MSG));
        msg->what = -1;
        msg->data = NULL;
        msg->data_size = 0;
        msg->handler = handler;
        msg->ev.next = NULL;
        msg->ev.prev = NULL;

        addToQueue(msg, &handler->msg_queue);
        ++handler->msg_count;
    }

    LEAVE_LOG();
    return msg;
}

/*
 * ��Ŀ��handler����Ϣ���л�ȡ��һ��MSG
 * ����:
 *     handler      - ��ǰhandler
 *     what         - ��Ϣ����
 *     data         - ����ϢЯ��������
 *     data_size    - ���ݴ�С
 * ����ֵ:
 *     �õ���Ϣ�����ػ�ȡ���� msg ָ��
 *     ʧ���򷵻� NULL
 */
MSG* obtain_message(HANDLER* handler, int what, void* data, int data_size)
{
    MSG* msg = NULL;
    ENTER_LOG();
    
    if (handler==NULL) {
        LEAVE_LOG();
        return NULL;
    }
    
    msg = obtain(handler);
    if (msg)
    {
        msg->what = what;
        if (data_size>0)
        {
            msg->data = malloc(data_size+2);
            if (msg->data) {
                memset(msg->data, 0, data_size+2);
                memcpy(msg->data, data, data_size);
                msg->data_size = data_size;
            } else {
                upil_error("malloc error: %d", errno);
                remove_message(msg);
                msg = NULL;
            }
        }
        if (msg)
            dump_msg("obtain new message:", msg);
    } else {
        upil_dbg("no msg cant be obtain.");
    }

    upil_printf(MSG_MSGDUMP, "now msg count: %d", handler->msg_count);

    LEAVE_LOG();
    return msg;
}

/*
 * ����һ����ʱ��Ϣ�� Handler �� loop ��
 * ����:
 *     msg          - Ŀ����Ϣ
 *     msec         - ��ʱʱ�䣬��λ��ms
 * ����ֵ:
 */
void send_message_delay(MSG* msg, uint msec)
{
    ENTER_LOG();

    if (msg==NULL) {
        LEAVE_LOG();
        return;
    }

    upil_printf(MSG_MSGDUMP, "Send msg: msg(%p) what(%d) delay(%dms)", msg, msg->what, msec);

    // ���һ��timeout event��loop�У�����process_message��Ϊ�ص�����
    uew_event_set(&msg->ev, -1, 0, msg->handler->msg_processor, (void*)msg);
    uew_timer_add_wakeup(&msg->handler->loop, &msg->ev, msec);

    LEAVE_LOG();
}

/*
 * ����һ���Ƴ���Ϣ������Ϣ�����´θı�״̬��״̬ʱ������
 * ����:
 *     msg          - Ŀ����Ϣ
 * ����ֵ:
 */
void defer_message(MSG* msg)
{
    ENTER_LOG();

    if (msg==NULL) {
        LEAVE_LOG();
        return;
    }

    removeFromQueue(msg);
    addToQueue(msg, &msg->handler->defer_msg_queue);

    LEAVE_LOG();
}

/*
 * ��������ԭ�����Ƴٵ���Ϣ
 * ����:
 *     msg          - Ŀ����Ϣ����
 * ����ֵ:
 */
void process_defer_msg(MSG* queue)
{
    MSG* msg = NULL;
    MSG* next = NULL;

    if (queue==NULL) {
        LEAVE_LOG();
        return;
    }

    msg = queue->next;

    ENTER_LOG();
    while(msg != queue)
    {
        upil_printf(MSG_MSGDUMP, "Remove msg=%p\n", msg);
        next = msg->next;
        removeFromQueue(msg);
        send_message(msg);
        msg = next;
    }

    LEAVE_LOG();
}

// ����Ϣ�������б����Ƴ���
void remove_from_list(MSG* msg)
{
    ENTER_LOG();
    if (msg==NULL) {
        LEAVE_LOG();
        return;
    }

    // ��loop�д����˸���Ϣ�󣬻��Զ�ɾ������߲�Ҫ�ظ�ɾ��
    if (msg->ev.next!=NULL && msg->ev.prev!=NULL)
    {
        upil_printf(MSG_EXCESSIVE, "Remove event=%p\n", &msg->ev);
        uew_event_del(&msg->handler->loop, &msg->ev, 1);
    } else
        upil_printf(MSG_EXCESSIVE, "msg->ev is empty!");

    removeFromQueue(msg);
    --msg->handler->msg_count;

    LEAVE_LOG();
}

/*
 * ��msg������ɺ�Ӧ���øú������黹msg�ռ�
 * ����:
 *     msg          - ���Ƴ�����Ϣ
 * ����ֵ:
 */
void remove_message(MSG* msg)
{
    ENTER_LOG();
    
    if (msg==NULL) {
        LEAVE_LOG();
        return;
    }

    upil_printf(MSG_MSGDUMP, "remove msg: %p\n", msg);
    
    remove_from_list(msg);
    
    upil_printf(MSG_MSGDUMP, "now msg count=%d", msg->handler->msg_count);

    free(msg);

    LEAVE_LOG();
}

void remove_message_id(MSG* queue, int what)
{
    MSG* msg = NULL;
    MSG* next;

    ENTER_LOG();

    upil_printf(MSG_MSGDUMP, "Remove message id %d from QUEUE(%p)\n", what, queue);
    
    if (queue==NULL) {
        LEAVE_LOG();
        return;
    }

    msg = queue->next;

    while(msg != queue)
    {
        next = msg->next;
        if (msg->what==what)
        {
            remove_message(msg);
        }
        msg = next;
    }

    LEAVE_LOG();
}

// ����handler�����ڴ��� message
int start_handler(HANDLER* handler, MSG_PROCESSOR func)
{
    int ret = 0;
    ENTER_LOG();

    upil_dbg("Start Event Loop");
    ret = uew_startEventLoop(&handler->loop);
    if (!ret)
    {
        upil_dbg("Init handler\n");
        init_queue(&handler->msg_queue);
        init_queue(&handler->defer_msg_queue);
        handler->msg_count = 0;
        handler->msg_processor = func;
    }
    else
        upil_error("*** Evnet loop startup failed!");
    
    LEAVE_LOG();
    return ret;
}

