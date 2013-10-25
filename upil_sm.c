/*
** Copyright 2012, albuer@gmail.com
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <upil_config.h>

#include "upil_sm.h"
#include "upil.h"
#include <upil_audio/upil_audio.h>


#define LOG_TAG "UPIL_SM"
//#define LOG_NDEBUG 0 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

/****  ��������  ***/

/****  ��������  ***/

#define USB_KEEP_ALIVE_TIMER        5000
#define SYNCING_TIMEOUT             2000
#define DIAL_DELAY                  1000
#define DIALINGTIMEOUT              6000
#define RING_TIMEOUT                8000
#define RECORD_START_TIMEOUT        24000

/****  �ڲ�����ʵ��  ***/

static void dump_fsm(FSM* fsm)
{
	if (MSG_DEBUG < upil_debug_level)
		return;

    int i=0;
    upil_dbg("--- FSM ---\n");
    upil_dbg("name: %s\n", fsm->name);
    upil_dbg("table size: %d\n", fsm->size);
    upil_dbg("table:\n");
    for (; i<fsm->size; i++)
    {
        upil_dbg("%d\t%s\t%d\n", i, fsm->table[i].name, fsm->table[i].id);
    }
    
    upil_dbg("current state: %p\n", fsm->cur_state);
    upil_dbg("handler owner: %p\n", fsm->handler.owner);
}

#define send_cmd_noparam(req)    tpp_send_cmd(req, NULL, 0)

static int default_process(FSM* fsm, MSG* msg)
{
    if (fsm->cur_state->id==UPIL_SM_OFFLINE) {
    } else {
        switch (msg->what)
        {
        case UPIL_CTL_USB_ON:
            send_cmd_noparam(UPIL_CTL_USB_ON);
            sm_send_message_delay(&fsm->handler,
                                    UPIL_CTL_USB_ON,
                                    NULL,
                                    0,
                                    USB_KEEP_ALIVE_TIMER);
            break;

        case UPIL_EVT_USB_DETACH:
            sm_transition_to(fsm, UPIL_SM_OFFLINE);
            break;

        default:
            break;
        }
    }

    return HANDLED;
}

static void offline_enter(FSM* fsm)
{
    // ����������״̬ʱ��ֹͣ����������
    sm_remove_message_id(fsm, UPIL_CTL_USB_ON);
    sendto_monitor(UPIL_STATE_OFFLINE, NULL, 0);
}

static int offline_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_EVT_USB_ATTACH:
        sm_transition_to(fsm, UPIL_SM_SYNCING);
        break;
    case UPIL_ASK_GET_VERSION:
        break;
    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static void syncing_enter(FSM* fsm)
{
    MSG* msg;

    sendto_monitor(UPIL_STATE_SYNCING, NULL, 0);

    // ����һϵ������
    send_cmd_noparam(UPIL_CTL_USB_ON);
    send_cmd_noparam(UPIL_ASK_HARDWARE_INF);
    send_cmd_noparam(UPIL_ASK_READ_SN);
    send_cmd_noparam(UPIL_ASK_GET_VERSION);
    send_cmd_noparam(UPIL_ASK_PHONE_STATE);
    send_cmd_noparam(UPIL_ASK_RECORD_UP);
    send_cmd_noparam(UPIL_CTL_RINGER_OFF);
    send_cmd_noparam(UPIL_CTL_SET_TIME);

    // ÿ��5�뷢��һ��CTL_USB_RUN��ָ����豸
    sm_send_message_delay(&fsm->handler,
                            UPIL_CTL_USB_ON,
                            NULL,
                            0,
                            USB_KEEP_ALIVE_TIMER);

    // ��ʼͬ��2������IDLE״̬
    sm_send_message_delay(&fsm->handler,
                            UPIL_EVT_SYNC_TIMEOUT,
                            NULL,
                            0,
                            SYNCING_TIMEOUT);
}

static int syncing_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_EVT_SYNC_TIMEOUT:
        sm_transition_to(fsm, UPIL_SM_IDLE);
        break;

    case UPIL_EVT_PHONE_STATE:
        sm_remove_message_id(fsm, UPIL_EVT_SYNC_TIMEOUT);
        sm_send_message_delay(&fsm->handler,
                                UPIL_EVT_SYNC_TIMEOUT,
                                NULL,
                                0,
                                SYNCING_TIMEOUT);
        break;

    case UPIL_EVT_HISTORY_FINISHED:
        sm_remove_message_id(fsm, UPIL_EVT_SYNC_TIMEOUT);
        sm_transition_to(fsm, UPIL_SM_IDLE);
        break;

    case UPIL_EVT_DIALED_TX:
        break;
    case UPIL_EVT_NEW_CID_TX:
        break;
    case UPIL_EVT_OLD_CID_TX:
        break;
    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static int s_ring_count = 0;

static void idle_enter(FSM* fsm)
{
    sendto_monitor(UPIL_STATE_IDLE, NULL, 0);

    // ֹͣ����
    upil_audio_stop();

    // ����û��������ʾ
    memset(fsm->last_incoming_num, 0, 128);

    s_ring_count = 0;
}

static int idle_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_CTL_HANDFREE_ON:
        // ���ժ������Ϣ��Я����Ŀ��绰���룬���ȱ�����
        // ���յ�ժ���¼���ʼ����
        memset(fsm->last_outgoing_num, 0, 128);
        if (msg->data) {
            sprintf(fsm->last_outgoing_num, "%s", (char*)msg->data);
            upil_info("Last outgoing number: %s", fsm->last_outgoing_num);
        }

        // ����ժ������
        send_cmd_noparam(UPIL_CTL_HANDFREE_ON);
        break;

    case UPIL_EVT_HFREE_HOOK_ON:
        sm_transition_to(fsm, UPIL_SM_DIALING);
        break;

    case UPIL_EVT_CALL_ID:
        sprintf(fsm->last_incoming_num, "%s", (char*)msg->data);
        upil_info("Last incoming number: %s", fsm->last_incoming_num);
        sendto_monitor(msg->what, msg->data, msg->data_size);
        sm_transition_to(fsm, UPIL_SM_INCOMING);
        break;

    case UPIL_EVT_RING:
        if (++s_ring_count>=2)
            sm_transition_to(fsm, UPIL_SM_INCOMING);
        break;
        
    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static void dialing_enter(FSM* fsm)
{
    sendto_monitor(UPIL_STATE_DIALING, NULL, 0);
    
    // ���ж��Ƿ��к��룬�к�������
    if (fsm->last_outgoing_num[0])
    {
        // ����dialing״̬����ʱ1s��ʼ����
        sm_send_message_delay(&fsm->handler,
                                UPIL_ASK_DTMF_TX,
                                fsm->last_outgoing_num,
                                strlen(fsm->last_outgoing_num),
                                DIAL_DELAY);
    }

    upil_audio_start();
}

static int dialing_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_ASK_DTMF_TX:
        tpp_send_cmd(msg->what, msg->data, msg->data_size);
        
        // 6������ conversation ״̬
        sm_send_message_delay(&fsm->handler,
                                UPIL_EVT_DIAL_END_CHECK,
                                NULL,
                                0,
                                DIALINGTIMEOUT);
        break;
        
    case UPIL_EVT_DIAL_END: // �������
    case UPIL_EVT_DIAL_END_CHECK:
        sm_transition_to(fsm, UPIL_SM_CONVERSATION);
        break;

    case UPIL_EVT_REDIAL:
        // �ڲ���״̬�У��յ������������Ϣ�������¼���6�볬ʱ
        sm_remove_message_id(fsm, UPIL_EVT_DIAL_END_CHECK);
        sm_send_message_delay(&fsm->handler,
                                UPIL_EVT_DIAL_END_CHECK,
                                NULL,
                                0,
                                DIALINGTIMEOUT);
        break;

    case UPIL_CTL_HANDFREE_OFF:
        send_cmd_noparam(msg->what);
        break;
        
    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static void incoming_enter(FSM* fsm)
{
    sendto_monitor(UPIL_STATE_INCOMING, NULL, 0);
    
    sm_remove_message_id(fsm, UPIL_EVT_RING_TIMEOUT);
    sm_send_message_delay(&fsm->handler,
                            UPIL_EVT_RING_TIMEOUT,
                            NULL,
                            0,
                            RING_TIMEOUT);

// ��ָ��ʱ����û�н��������Զ�ת������״̬
    sm_remove_message_id(fsm, UPIL_EVT_RECORD_START);
    sm_send_message_delay(&fsm->handler,
                            UPIL_EVT_RECORD_START,
                            NULL,
                            0,
                            RECORD_START_TIMEOUT);

    // ��������״̬����������
    // ���ϲ㲥���趨����������
//    upil_audio_start();
}

static int incoming_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_CTL_HANDFREE_OFF:
        send_cmd_noparam(msg->what);
        break;

    case UPIL_CTL_HANDFREE_ON:
        send_cmd_noparam(msg->what);
        break;

    case UPIL_EVT_HAND_HOOK_ON:
    case UPIL_EVT_HFREE_HOOK_ON:
    case UPIL_EVT_PARALL_HOOK_ON:
        sm_transition_to(fsm, UPIL_SM_CONVERSATION);
        break;
        
    case UPIL_EVT_RING:
        // ����ȡ�������¼�ʱ���ɽ����¼��ϱ����ϲ�
        // ���ϲ㲥����������
        sendto_monitor(UPIL_EVT_RING, NULL, 0);

        sm_remove_message_id(fsm, UPIL_EVT_RING_TIMEOUT);
        sm_send_message_delay(&fsm->handler,
                                UPIL_EVT_RING_TIMEOUT,
                                NULL,
                                0,
                                RING_TIMEOUT);
        break;

    case UPIL_EVT_CALL_ID:
        sprintf(fsm->last_incoming_num, "%s", (char*)msg->data);
        upil_info("Last incoming number: %s", fsm->last_incoming_num);
        sendto_monitor(msg->what, msg->data, msg->data_size);
        break;

    case UPIL_EVT_RING_TIMEOUT:
        sm_transition_to(fsm, UPIL_SM_IDLE);
        break;

    case UPIL_EVT_RECORD_START:
        sm_transition_to(fsm, UPIL_SM_MEMO);
        break;

    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static void conversation_enter(FSM* fsm)
{
    sendto_monitor(UPIL_STATE_CONVERSATION, NULL, 0);

    upil_audio_start();
    
    // �������������Է���
    send_cmd_noparam(UPIL_CTL_RECORD_LINE_PLAY_ON);
    // ����æ�����
    send_cmd_noparam(UPIL_ASK_BUSY_TONE);
}

static int conversation_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_CTL_HANDFREE_OFF:
        send_cmd_noparam(msg->what);
        break;

    case UPIL_EVT_HFREE_HOOK_OFF:
    case UPIL_EVT_HAND_HOOK_OFF:
        send_cmd_noparam(UPIL_CTL_RECORD_LINE_PLAY_OFF);
        sm_transition_to(fsm, UPIL_SM_IDLE);
        break;

    // ��⵽�Է��һ�ʱ���Ҷϱ���
    case UPIL_EVT_BUSY_ON:
        upil_sm_hangup(fsm);
        break;

    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

static void begin_record()
{
    upil_info("Begin record...\n");
    sm_send_message(&g_upil_s.fsm->handler,
                            UPIL_CTL_RECORD_START,
                            NULL,
                            0);
}

static void end_record()
{
    upil_info("End record...\n");
    sm_send_message(&g_upil_s.fsm->handler,
                            UPIL_EVT_RECORD_END,
                            NULL,
                            0);
}

static void stop_memo_flow()
{
    upil_audio_stop();
}

static void memo_enter(FSM* fsm)
{
    sendto_monitor(UPIL_STATE_MEMO, NULL, 0);

    // �Զ�ժ��
    send_cmd_noparam(UPIL_CTL_HANDFREE_ON);
}

static int memo_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case UPIL_EVT_HAND_HOOK_ON:
    case UPIL_EVT_HFREE_HOOK_ON:
    case UPIL_EVT_PARALL_HOOK_ON:
        // �������������Է���
        send_cmd_noparam(UPIL_CTL_RECORD_LINE_PLAY_ON);
        // ����æ�����
        send_cmd_noparam(UPIL_ASK_BUSY_TONE);
        // ������ʾ��
        upil_audio_play_file("/data/welcome.wav", begin_record);
        break;

    case UPIL_CTL_HANDFREE_ON:
        // �û������绰��������ֹ�������̣�Ȼ�����ͨ��״̬
        stop_memo_flow();
        send_cmd_noparam(UPIL_CTL_HANDFREE_ON);
        sm_transition_to(fsm, UPIL_SM_CONVERSATION);
        break;

    case UPIL_CTL_HANDFREE_OFF:// �û��ֶ��Ҷϵ绰
        stop_memo_flow();
    case UPIL_EVT_RECORD_END:// ¼���������Զ��Ҷϵ绰
        send_cmd_noparam(UPIL_CTL_HANDFREE_OFF);
        break;

    case UPIL_EVT_HFREE_HOOK_OFF:
    case UPIL_EVT_HAND_HOOK_OFF:
        send_cmd_noparam(UPIL_CTL_RECORD_LINE_PLAY_OFF);
        upil_audio_stop();
        sm_transition_to(fsm, UPIL_SM_IDLE);
        break;

    // ��⵽�Է��һ�ʱ���Ҷϱ���
    case UPIL_EVT_BUSY_ON:
        upil_sm_hangup(fsm);
        break;

    case UPIL_CTL_RECORD_START:
        upil_audio_record_file("/data/record.wav", end_record, 30);
        break;

    default:
        default_process(fsm, msg);
        break;
    }

    return HANDLED;
}

BEGIN_FSM_STATE_TABLE(my_state_table)
    ADD_STATE(UPIL_SM_OFFLINE,  "Offline", offline_enter, NULL, offline_process)
    ADD_STATE(UPIL_SM_SYNCING,  "Syncing", syncing_enter, NULL, syncing_process)
    ADD_STATE(UPIL_SM_IDLE,     "Idle", idle_enter, NULL, idle_process)
    ADD_STATE(UPIL_SM_DIALING,  "Dialing", dialing_enter, NULL, dialing_process)
    ADD_STATE(UPIL_SM_INCOMING, "Incoming", incoming_enter, NULL, incoming_process)
    ADD_STATE(UPIL_SM_CONVERSATION,  "Conversation", conversation_enter, NULL, conversation_process)
    ADD_STATE(UPIL_SM_MEMO,  "Memo", memo_enter, NULL, memo_process)
END_FSM_STATE_TABLE(my_state_table)


void foo(void) {}

/****  ���⺯��ʵ��  ***/

// �ṩ�Ĺ���ͨ�����к�����ʵ��

int upil_sm_get_current_state(FSM* fsm)
{
    if (fsm && fsm->cur_state)
        return fsm->cur_state->id;

    return UPIL_SM_OFFLINE;
}

/*
 * ���ŵ绰
 * ����:
 *     number    - Ŀ��绰����
                   ��û��ָ���绰����ʱ��number=NULL
 * ����ֵ:
 *     1    �������
 *     0    ����δ����
 */
int upil_sm_start_dail(FSM* fsm, const char* number)
{
    return sm_send_message(&fsm->handler,
                            UPIL_CTL_HANDFREE_ON,
                            (void*)number,
                            number?strlen(number):0);
}

int upil_sm_hangup(FSM* fsm)
{
    return sm_send_message(&fsm->handler,
                            UPIL_CTL_HANDFREE_OFF,
                            NULL,
                            0);
}

int upil_sm_usb_attach(FSM* fsm)
{
    return sm_send_message(&fsm->handler,
                            UPIL_EVT_USB_ATTACH,
                            NULL,
                            0);
}

/*
    reply   ���ػ�ȡ������Ϣ
    ����ֵ
        0   -   ָ��ִ����ɻ���Ҫִ��ָ��
        -1  -   ָ��ִ��ʧ��
 */
int upil_sm_get_version(FSM* fsm, char** reply)
{
    int ret = 0;
    char* this_version = "1.2.3";

    if (this_version)
    {
        asprintf(reply, "%s", this_version);
        ret = 0;
    }
    else
    {
        ret = sm_send_message(&fsm->handler,
                                UPIL_ASK_GET_VERSION,
                                NULL,
                                0);
    }

    return ret?-1:0;
}

int upil_sm_send_msg(FSM* fsm, int event, void* data, int data_size)
{
    return sm_send_message(&fsm->handler,
                            event,
                            data,
                            data_size);
}

/*
 * ����UPIL״̬��
 * ����:
 *     name ״̬������
 * ����ֵ:
 *     �����ɹ��������·����״̬��
 *     ����ʧ�ܣ�����NULL
 */
FSM* upil_sm_start(const char* name)
{
    FSM* fsm = NULL;
    ENTER_LOG();
    
    fsm = sm_obtain_fsm(&my_state_table[0], sizeof(my_state_table)/sizeof(FSM_STATE));
    if (fsm)
    {
        sm_set_init_state(fsm, UPIL_SM_OFFLINE);
        strcpy(fsm->name, name);
        dump_fsm(fsm);
    }
    else
        upil_error("*** can not obtain FSM");

    LEAVE_LOG();
    return fsm;
}

