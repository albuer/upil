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

/*
    创建 socket: /dev/socket/upil.ril，然后等待外部程序的连接
    当外部程序连接到该socket之后，外部程序就可以通过该socket发送AT指令，
    并从该socket获取到AT指令的返回及主动上报
    可实现拨号、接听等功能
 */
#include <stdio.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <fcntl.h>

#include <upil_config.h>
#include "upil.h"

#define MAX_AT_RESPONSE     (8 * 1024)
#define MAX_AT_COMMAND      (8 * 1024)

#define UPIL_AT_DIAL        "ATD"
#define UPIL_AT_DIAL_LEN    strlen(UPIL_AT_DIAL)

#define UPIL_AT_ANSWER      "ATA"

#define UPIL_AT_HANGUP      "AT+CHLD="

#define UPIL_AT_CURRENT_CALLS   "AT+CLCC"

#define UPIL_AT_CHECK           "ATE0"

static int s_fd_ril = -1;

static int s_call_dir = 0;
static int s_wait_response = 0;

static char s_at_cmd_buf[MAX_AT_COMMAND] = "";

static struct upil_event s_listen_event;
static struct upil_event s_commands_event;

static uew_context* s_event_loop=NULL;


extern int strStartsWith(const char *line, const char *prefix);

/*
 * 把命令的处理结果返回给外部程序
 * 参数:
 *     fd   - 双方通信的socket
 *     result - 命令执行成功，则为0
                命令处于执行中，返回1
                否则为<0
 *     reply - 返回的消息
 * 返回值:
 */
static void process_reply(int fd, int result, const char* reply)
{
    ENTER_LOG();

    if (reply)
    {
        upil_dbg("UPIL_AT << %s", reply);
        write(fd, reply, strlen(reply));
        free((void*)reply);
    }

    if (result<1)
    {
        char* str_result = result?"ERROR\n\0":"OK\n\0";

        upil_dbg("UPIL_AT << %s", str_result);
        write(fd, str_result, strlen(str_result)+1);
    }

    LEAVE_LOG();
}

// 处理接收到的命令
static void processCommandBuffer(int fd, char *buffer, size_t buflen)
{
    char* reply=NULL;
    int result = 0;

    ENTER_LOG();

    upil_dbg("UPIL_AT >> %s", buffer);

    if (!strcmp(buffer,"\r"))
        return;

    // 执行指令
    if (strStartsWith(buffer, UPIL_AT_CHECK))
    {
        result = (upil_sm_get_current_state(g_upil_s.fsm)<UPIL_SM_IDLE)?-1:0;
    }
    if (strStartsWith(buffer, UPIL_AT_DIAL))
    {
        /*
            发送 UPIL_CTL_HANDFREE_ON 之后， 先不给rild返回OK
            直到 收到 UPIL_STATE_DIALING 消息后， 再返回OK
         */
        char* dial_num = buffer+UPIL_AT_DIAL_LEN;
        // 去掉末尾的换行符
        dial_num[strlen(dial_num)-1]='\0';
        result = upil_sm_start_dail(g_upil_s.fsm, dial_num[0]?dial_num:NULL);
        if (result == 0) {
            result = 1;
            s_wait_response = 1;
        }
    }
    else if (strStartsWith(buffer, UPIL_AT_HANGUP))
    {
        /*
            发送 UPIL_CTL_HANDFREE_OFF 之后， 先不给rild返回OK
            直到 收到 UPIL_STATE_IDLE 消息后， 再返回OK
         */
        result = upil_sm_hangup(g_upil_s.fsm);

        if (result == 0) {
            result = 1;
            s_wait_response = 1;
        }
    }
    else if (strStartsWith(buffer, UPIL_AT_ANSWER))
    {
        result = upil_sm_start_dail(g_upil_s.fsm, NULL);
    }
    else if (strStartsWith(buffer, UPIL_AT_CURRENT_CALLS))
    {
        int cur_state = upil_sm_get_current_state(g_upil_s.fsm);
        int state;

        if (cur_state>UPIL_SM_IDLE) {
            switch (cur_state)
            {
            case UPIL_SM_DIALING:
                s_call_dir = 0;
                state = 2;
                break;
            case UPIL_SM_INCOMING:
                s_call_dir = 1;
                state = 4;
                break;
            case UPIL_SM_CONVERSATION:
            case UPIL_SM_MEMO:
                state = 0;
                break;
            }
            
            asprintf(&reply, "+CLCC: 1,%d,%d,0,0,\"%s\",129\n", s_call_dir, state, 
                    s_call_dir?g_upil_s.fsm->last_incoming_num:g_upil_s.fsm->last_outgoing_num);
        }
    }

    // 处理命令的返回值
    process_reply(fd, result, reply);

    LEAVE_LOG();
    
    return;
}

static int processCommandsCallback(int fd, short flags, void *param)
{
    int ret = 0;

    char* cmd_buf = (char*)param;

    ENTER_LOG();

    memset(cmd_buf, 0, MAX_AT_COMMAND);
    ret = recv(fd, cmd_buf, MAX_AT_COMMAND-1, 0);
    
    if(ret > 0)
    {
        processCommandBuffer(fd, cmd_buf, ret);
    }
    else
    {
        upil_warn("socket disconnected\n");
        close(fd);

        uew_event_del(s_event_loop, &s_commands_event, 0);

        /* start listening for new connections again */
        uew_event_add_wakeup(s_event_loop, &s_listen_event);
    }

    LEAVE_LOG();

    return 0;
}

/*
 * 接受其它socket的连接，并把该连接加入event loop
 * 参数:
 *     event_loop   - command socket 将添加到该event loop中
 * 返回值:
 *     成功返回 0
 *     出现异常，则返回 -1
 */
static int listenCallback (int fd, short flags, void *param) {
    int ret;
    int fd_cmd=-1;

    ENTER_LOG();
    
    fd_cmd = accept(fd, NULL, NULL);

    if (fd_cmd < 0 ) {
        upil_error("Error on accept() errno:%d\n", errno);
        /* start listening for new connections again */
        uew_event_add_wakeup(&g_upil_s.event_loop, &s_listen_event);
        LEAVE_LOG();
        return -1;
    }

    ret = fcntl(fd_cmd, F_SETFL, O_NONBLOCK);

    if (ret < 0) {
        upil_error("Error setting O_NONBLOCK errno:%d\n", errno);
    }

    uew_event_set(&s_commands_event, fd_cmd, 1,
        processCommandsCallback, s_at_cmd_buf);

    uew_event_add_wakeup(s_event_loop, &s_commands_event);

    s_fd_ril = fd_cmd;
    
    LEAVE_LOG();

    return 0;
}

void upil_ril_unsolicited(int msg)
{
    char buf[MAX_AT_RESPONSE] = "";

    upil_dbg("upil_ril_unsolicited %d", msg);

    switch (msg) {
    case UPIL_STATE_INCOMING:
        sprintf(buf, "%s\n", "RING");
        send(s_fd_ril, buf, strlen(buf)+1, 0);
        break;

    case UPIL_STATE_IDLE:
        if (!s_wait_response)
        {
            sprintf(buf, "%s\n", "NO CARRIER");
            send(s_fd_ril, buf, strlen(buf)+1, 0);
        } else {
            process_reply(s_fd_ril, 0, NULL);
            s_wait_response = 0;
        }
        break;

    case UPIL_STATE_DIALING:
        if (s_wait_response) {
            process_reply(s_fd_ril, 0, NULL);
            s_wait_response = 0;
        }
        break;
    }
}

/*
 * 与ril库配合，用来处理AT指令
 * 参数:
 *     event_loop   - command socket 将添加到该event loop中
 * 返回值:
 *     成功返回 0
 *     出现异常，则返回 -1
 */
int upil_ril_setup(void* event_loop)
{
    int fd;
    ENTER_LOG();

    if (event_loop==NULL)
    {
        upil_error("event loop can not be empty!");
        LEAVE_LOG();
        return -1;
    }

    unlink(UPIL_SOCKET_RIL_PATH);

    // add listen to event loop for cmd channel connect
    fd = socket_local_server (UPIL_SOCKET_RIL, 1, SOCK_STREAM);

    if (fd < 0) {
        upil_error("Unable to bind socket: %s\n", strerror(errno));
        LEAVE_LOG();
        return -1;
    }
    s_event_loop = (uew_context*)event_loop;
    
    uew_event_set(&s_listen_event, fd, 0, listenCallback, NULL);
    
    uew_event_add_wakeup(s_event_loop, &s_listen_event);

    LEAVE_LOG();
    return 0;
}

