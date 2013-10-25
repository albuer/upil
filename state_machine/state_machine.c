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

#define LOG_TAG "UPIL_SM"
//#define LOG_NDEBUG 0 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

#include "state_machine.h"
#include "upil.h"

#define MAX_STATE_MACHINE   64

static FSM s_sm_pool[MAX_STATE_MACHINE];

static const char *event_str[] = UPIL_EVENT_STRING_TABLE;
static const char *ctl_str[] = UPIL_CTL_STRING_TABLE;
static const char *unknown_msg = "unknown";

extern void remove_from_list(MSG* msg);

const char* msg2str(int msg)
{
    const char* ret = unknown_msg;
    
    if (msg>=UPIL_CTL_BEGIN && msg<UPIL_CTL_MAX)
        ret = ctl_str[msg-UPIL_CTL_BEGIN];
    else if (msg>=UPIL_EVT_BEGIN && msg<UPIL_EVT_MAX)
        ret = event_str[msg-UPIL_EVT_BEGIN];

    return ret;
}

/*
 * 获取指定状态ID的状态
 * 参数:
 *     fsm      - 目标状态机
 *     id       - 要查找到状态ID
 * 返回值:
 *     找到则返回状态
 *     找不到则返回 NULL
 */
static FSM_STATE* get_state(FSM* fsm, FSM_STATE_ID id)
{
    int i=0;
    for (;i<fsm->size;i++)
        if (fsm->table[i].id==id) return &(fsm->table[i]);

    return NULL;
}

/*
 * 获取空闲的状态机
 * 参数:
 * 返回值:
 *     返回空闲状态机的索引值
 *     没有空闲状态机，则返回 -1
 */
static FSM* get_idle_fsm()
{
    int i=0;
    for (;i<MAX_STATE_MACHINE;i++)
        if (!s_sm_pool[i].table) return &s_sm_pool[i];

    return NULL;
}

/*
 * 状态转换
 *     先退出当前状态的退出函数，再调用新状态的进入函数
 * 参数:
 *     fsm      - 目标状态机
 *     id       - 要转换在状态
 * 返回值:
 *     转换成功或者无须转换，则返回HANDLED
 *     无法找到新状态，则返回NOT_HANDLED
 */
static int transition_to(FSM* fsm, FSM_STATE_ID id)
{
    FSM_STATE* new_state;
    ENTER_LOG();

    if (fsm->cur_state && fsm->cur_state->id == id)
    {
        upil_warn("Same state, not transition\n");
        LEAVE_LOG();
        return HANDLED;
    }

    new_state = get_state(fsm, id);

    if (!new_state)
    {
        upil_error("unknown state(%d), can't transition to it\n", id);
        LEAVE_LOG();
        return NOT_HANDLED;
    }
    
    upil_dbg("[%s]: State transition [%s] -> [%s]\n", 
                        fsm->name, 
                        fsm->cur_state?fsm->cur_state->name:"NULL",
                        new_state->name);
    
    if (fsm->cur_state && fsm->cur_state->exit_func)
    {
        upil_dbg("Execute exit func");
        fsm->cur_state->exit_func(fsm);
    }

    // 转换到新的状态
    fsm->cur_state = new_state;

    if (fsm->cur_state->enter_func)
    {
        upil_dbg("Execute enter func");
        fsm->cur_state->enter_func(fsm);
    }

    // 处理被推迟的消息
    process_defer_msg(&fsm->handler.defer_msg_queue);

    LEAVE_LOG();
    return HANDLED;
}

/*
 * 将事件分配给指定状态机的当前状态去处理
 * 参数:
 *     fsm      - 目标状态机
 *     event    - 事件
 * 返回值:
 *     处理完成，返回 HANDLED
 *     无法处理，返回 NOT_HANDLED
 */
static int process_msg(FSM* fsm, MSG* msg)
{
    int ret = HANDLED;
    ENTER_LOG();

    if (fsm->cur_state && fsm->cur_state->process_func)
    {
        upil_dbg("[%s]: in [%s] process %s(%d)\n", fsm->name, 
                                    fsm->cur_state->name, 
                                    msg2str(msg->what), 
                                    msg->what);
        // 调用当前状态机的事件处理函数来处理该消息
        ret = fsm->cur_state->process_func(fsm, msg);
    }

    LEAVE_LOG();
    return ret;
}

/*
    状态机的所有消息都将在此分发到各个状态去处理
        正常返回 0
        当返回值 <0 时，将会退出状态机
 */
int sm_process_message(int fd, short flags, void *param)
{
    int ret = HANDLED;
    MSG* msg = (MSG*)param;
    ENTER_LOG();

    upil_dbg("Process msg(%p) event(%p) what(%d)", msg, &msg->ev, msg->what);

    // 消息从队列中移除
    remove_from_list(msg);

    if (msg->what==UPIL_EVT_SM_TRANSITION)
        // 状态转换
        ret = transition_to((FSM*)msg->handler->owner, *((int*)msg->data));
    else
        // 处理消息
        ret = process_msg((FSM*)msg->handler->owner, msg);

    upil_printf(MSG_MSGDUMP, "free msg(%p)", msg);
    // 释放
    free(msg);

    LEAVE_LOG();

    return (ret==HANDLED)?0:-1;
}

/*
    返回值:
        0   -   消息发送成功
        -1  -   消息发送失败
 */
int sm_send_message_delay(HANDLER* handler, int what, void* data, int data_size, int msec)
{
    MSG* msg = NULL;
    ENTER_LOG();
    
    msg = sm_obtain_message(handler,
                                    what,
                                    data,
                                    data_size);

    if (msg)
        send_message_delay(msg, msec);

    LEAVE_LOG();
    return msg?0:-1;
}

/*
 * 状态转换函数
 * 参数:
 *     fsm      - 目标状态机
 *     id       - 目标状态
 * 返回值:
 *     处理完成，返回 HANDLED
 *     无法处理，返回 NOT_HANDLED
 */
int sm_transition_to(FSM* fsm, FSM_STATE_ID id)
{
    int ret = 0;
    ENTER_LOG();
    
    ret = sm_send_message_delay(&fsm->handler, UPIL_EVT_SM_TRANSITION, (void*)&id, sizeof(id), 0);

    LEAVE_LOG();
    return ret;
}

int sm_set_init_state(FSM* fsm, FSM_STATE_ID id)
{
    int ret = 0;
    ENTER_LOG();
    
    ret = transition_to(fsm, id);
    
    LEAVE_LOG();
    return ret;
}

// 从FSM中删除掉指定消息ID的所有消息
void sm_remove_message_id(FSM* fsm, int what)
{
    remove_message_id(&fsm->handler.msg_queue, what);
    remove_message_id(&fsm->handler.defer_msg_queue, what);
}

/*
 * 通过该函数获取一个空闲的状态机
 * 参数:
 *     table    - 状态机的状态列表
 *     size     - 状态列表大小
 * 返回值:
 *     返回分配到的状态机
 *     分配失败，则返回 NULL
 */
FSM* sm_obtain_fsm(FSM_STATE* table, int size)
{
    FSM* fsm = NULL;
    ENTER_LOG();

    upil_info("obtain FSM.");
    
    if ((fsm=get_idle_fsm()) != NULL)
    {
        upil_info("start handler for the FSM.");
        if (start_handler(&fsm->handler, sm_process_message))
        {
            LEAVE_LOG();
            return NULL;
        }

        fsm->handler.owner = (void*)fsm;

        fsm->table = table;
        fsm->size = size;
        fsm->cur_state = NULL;
        fsm->name[0] = '\0';
    }
    else
        upil_error("*** No IDLE FSM!");

    LEAVE_LOG();
    return fsm;
}

// 在进程启动时，比如main函数中，初始化状态机池
void sm_init_pool(void)
{
    int i=0;
    ENTER_LOG();

    upil_info("Init FSM pool");
    
    for (;i<MAX_STATE_MACHINE;i++)
    {
        s_sm_pool[i].name[0] = '\0';
        s_sm_pool[i].table = NULL;
        s_sm_pool[i].size = 0;
        s_sm_pool[i].cur_state = NULL;
    }

    LEAVE_LOG();
}

