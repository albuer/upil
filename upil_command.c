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
    ���� socket: /dev/socket/upil.cmd��Ȼ��ȴ��ⲿ���������
    ���ⲿ�������ӵ���socket֮���ⲿ����Ϳ���ͨ����socket��ָ���upil��
    ��ʵ�ֲ��š������ȹ���
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
 * ������Ĵ��������ظ��ⲿ����
 * ����:
 *     fd   - ˫��ͨ�ŵ�socket
 *     result - ����ִ�гɹ�����Ϊ0�� ����Ϊ����ֵ
 *     reply - ���ص���Ϣ
 * ����ֵ:
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

// ������յ�������
static void processCommandBuffer(int fd, char *buffer, size_t buflen)
{
    char* reply=NULL;
    int result = 0;

    ENTER_LOG();

    upil_info("CTRL >> %s", buffer);

    // ��������
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

    // ��������ķ���ֵ
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
 * ��������socket�����ӣ����Ѹ����Ӽ���event loop
 * ����:
 *     event_loop   - command socket ����ӵ���event loop��
 * ����ֵ:
 *     �ɹ����� 0
 *     �����쳣���򷵻� -1
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
 * ������������������ⲿ��������ִ��
 * ����:
 *     event_loop   - command socket ����ӵ���event loop��
 * ����ֵ:
 *     �ɹ����� 0
 *     �����쳣���򷵻� -1
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

