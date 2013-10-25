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
    创建 socket: /dev/socket/upil.cmd，然后等待外部程序的连接
    当外部程序连接到该socket之后，外部程序就可以通过该socket发指令给upil，
    可实现拨号、接听等功能
 */
#include <stdio.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <fcntl.h>

#include <upil_config.h>
#include "upil.h"

static struct upil_event s_listen_event;
static struct upil_event s_commands_event;

static uew_context* s_event_loop=NULL;

static char s_cmd_buf[UPIL_MAX_MSG_LEN] = "";

/*
 * 把命令的处理结果返回给外部程序
 * 参数:
 *     fd   - 双方通信的socket
 *     result - 命令执行成功，则为0， 否则为其它值
 *     reply - 返回的消息
 * 返回值:
 */
static void process_reply(int fd, int result, const char* reply)
{
    char reply_buf[UPIL_MAX_MSG_LEN] = "";
    
    ENTER_LOG();
    
    snprintf(reply_buf, UPIL_MAX_MSG_LEN, "%s%s", 
                result?REPLY_FAILURE:REPLY_SUCCESS, 
                reply?reply:"");
    
    if (reply) free((void*)reply);

    upil_info("CTRL << %s", reply_buf);
    send(fd, reply_buf, strlen(reply_buf), 0);

    LEAVE_LOG();
}

// 处理接收到的命令
static void processCommandBuffer(int fd, char *buffer, size_t buflen)
{
    char* reply=NULL;
    int result = 0;

    ENTER_LOG();

    upil_info("CTRL >> %s", buffer);

    // 发送命令
    if (!strncmp(buffer, UPIL_CTRL_GET_VER, UPIL_CTRL_GET_VER_LEN))
    {
        result = upil_sm_get_version(g_upil_s.fsm, &reply);
        upil_info("reply: %s", reply);
    }
    else if (!strncmp(buffer, UPIL_CTRL_DIAL, UPIL_CTRL_DIAL_LEN))
    {
        char* dial_num = buffer+UPIL_CTRL_DIAL_LEN;
        result = upil_sm_start_dail(g_upil_s.fsm, dial_num[0]?dial_num:NULL);
    }
    else if (!strncmp(buffer, UPIL_CTRL_HANGUP, UPIL_CTRL_HANGUP_LEN))
    {
        result = upil_sm_hangup(g_upil_s.fsm);
    }
    else if (!strncmp(buffer, UPIL_CTRL_GET_STATE, UPIL_CTRL_GET_STATE_LEN))
    {
        asprintf(&reply, "%d", upil_sm_get_current_state(g_upil_s.fsm));
        result = 0;
        upil_info("reply: %s", reply);
    }
    else
        return;

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

    memset(cmd_buf, 0, UPIL_MAX_MSG_LEN);
    ret = recv(fd, cmd_buf, UPIL_MAX_MSG_LEN-1, 0);
    
    if(ret > 0)
    {
        processCommandBuffer(fd, cmd_buf, ret);
    }
    else
    {
        upil_warn("command socket disconnected\n");
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
        processCommandsCallback, s_cmd_buf);

    uew_event_add_wakeup(s_event_loop, &s_commands_event);

    LEAVE_LOG();

    return 0;
}

/*
 * 设置命令处理器，接收外部程序的命令并执行
 * 参数:
 *     event_loop   - command socket 将添加到该event loop中
 * 返回值:
 *     成功返回 0
 *     出现异常，则返回 -1
 */
int setup_cmd_processor(void* event_loop)
{
    int fd;
    ENTER_LOG();

    if (event_loop==NULL)
    {
        upil_error("event loop can not be empty!");
        LEAVE_LOG();
        return -1;
    }

    unlink(UPIL_SOCKET_CMD_PATH);

    // add listen to event loop for cmd channel connect
    fd = socket_local_server (UPIL_SOCKET_CMD, 1, SOCK_STREAM);

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

