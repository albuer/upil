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
    外部程序通过该辅助代码实现与UPIL的通信
 */
 
#include <stdio.h>
#include <string.h>
#include <cutils/sockets.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>

#include <upil_config.h>
#include "upil.h"
#include <upil_ctrl.h>

#define LOG_TAG "UPIL_CTRL"
#include <utils/Log.h>


static int s_fd_sender = -1;
static int s_fd_monitor = -1;
static int s_fd_wakeup = -1;

/*
    发送请求给upil，upil处理请求并返回结果:
        [SUCCESS]msg    指令处理完成，并有信息返回
        [SUCCESS]       指令处理完成，但没有信息返回
        [FAILURE]msg    指令无法处理，并有出错信息返回
        [FAILURE]       指令无法处理，也没有信息返回
    返回值:
        0   指令发送成功
        <0  指令发送失败
 */
int upil_ctrl_request(const char *cmd, char *reply, size_t reply_len)
{
	struct timeval tv;
	int res;
	fd_set rfds;
    char msg[UPIL_MAX_MSG_LEN];
    int msg_size = 0;
    int ret;

    reply[0] = '\0';

    upil_info("Send CMD: %s", cmd);

	if (send(s_fd_sender, cmd, strlen(cmd), 0) < 0) {
        upil_error("command send failure.");
		return -1;
	}

/*
    返回值格式:
        成功: "[SUCCESS]返回字符串"
        失败: "[FAILURE]出错信息"
 */
	for (;;) {
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(s_fd_sender, &rfds);
		res = select(s_fd_sender + 1, &rfds, NULL, NULL, &tv);
		if (res < 0)
			return -2;
		if (FD_ISSET(s_fd_sender, &rfds)) {
			res = recv(s_fd_sender, msg, UPIL_MAX_MSG_LEN-1, 0);
			if (res < 0)
				return -2;
			msg_size = res;
            msg[msg_size] = '\0';
			break;
		} else {
			return -2;
		}
	}

    upil_info("Get reply: %s", msg);

    if (!strncmp(msg, REPLY_SUCCESS, REPLY_SUCCESS_LEN))
    {
        strcpy(reply, msg+REPLY_SUCCESS_LEN);
        ret = 0;
    }
    else if (!strncmp(msg, REPLY_FAILURE, REPLY_FAILURE_LEN))
    {
        strcpy(reply, msg+REPLY_FAILURE_LEN);
        ret = -3;
    }
    else
    {
        strcpy(reply, msg);
        ret = -3;
    }

	return ret;
}

/*
 * 拨打电话
 * 参数:
 *     number   - 目标电话号码
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_ctrl_dial(const char* number)
{
    char cmd[64] = "";
    char replay[10];

    if (number==NULL)// 接听电话
        sprintf(cmd, "%s", UPIL_CTRL_DIAL);
    else// 拨打电话
        sprintf(cmd, "%s%s", UPIL_CTRL_DIAL, number);
    if (upil_ctrl_request(cmd, replay, 10) < 0)
    {
        return -1;
    }

    return 0;
}

/*
 * 挂断电话
 * 参数:
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_ctrl_hangup()
{
    char reply[128]="";
    if (upil_ctrl_request(UPIL_CTRL_HANGUP, reply, 128) < 0)
    {
        return -1;
    }
    return 0;
}

/*
    返回值:
        成功读到版本信息(可能为空)，则返回0，否则返回负数
 */
int upil_ctrl_get_version(char* version, int version_len)
{
    char reply[128]="";

    if (upil_ctrl_request(UPIL_CTRL_GET_VER, reply, 128) < 0)
    {
        return -1;
    }

    if (reply[0])
    {
        strcpy(version, reply);
        version_len = strlen(version);

        upil_dbg("Version: %s", version);
    }

    return 0;
}

int upil_ctrl_get_state(int* p_state)
{
    char reply[128]="";

    if (upil_ctrl_request(UPIL_CTRL_GET_STATE, reply, 128) < 0)
    {
        return -1;
    }

    upil_info("reply=%s", reply);

    if (reply[0])
    {
        *p_state = atoi(reply);
        upil_info("Current state: %d", *p_state);
    }

    return 0;
}

/*
 * 通过该函数与UPIL的command socket建立连接
 * 参数:
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_ctrl_connect()
{
    int ret;
    struct sockaddr_un addr;

    upil_info("Connect to upil ctrl\n");

    s_fd_sender = socket(PF_LOCAL, SOCK_STREAM, 0);
    if(s_fd_sender<=0)
    {
        upil_error("create socket failed: %d", errno);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    
    upil_dbg("upil cmd socket: %s", UPIL_SOCKET_CMD_PATH);
    snprintf(addr.sun_path, sizeof(addr.sun_path), UPIL_SOCKET_CMD_PATH);
    
    ret = connect(s_fd_sender, (struct sockaddr *)&addr, sizeof(addr)); 
    if(ret < 0)
    {
        upil_error("connect failed: %d", errno);
        return -1;
    } 
    
    ret = fcntl(s_fd_sender, F_SETFL, O_NONBLOCK);
    if (ret < 0)
    {
        upil_warn("Error setting O_NONBLOCK errno:%d", errno);
    } 

    return 0;
}

/*
 * 断开与UPIL的 command socket 的连接
 * 参数:
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_ctrl_disconnect()
{
    upil_info("Disconnect from upil ctrl\n");
    
	if (s_fd_sender >= 0)
		close(s_fd_sender);

    s_fd_monitor = -1;

    return 0;
}

/*
 * 通过该函数与UPIL的monitor socket建立连接
 * 参数:
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_monitor_connect()
{
    int ret;
    struct sockaddr_un addr;

    upil_info("Connect to upil monitor\n");

    s_fd_monitor = socket(PF_LOCAL, SOCK_STREAM, 0);
    if(s_fd_monitor<=0)
    {
        upil_error("create socket failed: %d", errno);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    
    upil_dbg("upil monitor socket: %s", UPIL_SOCKET_EVT_PATH);
    snprintf(addr.sun_path, sizeof(addr.sun_path), UPIL_SOCKET_EVT_PATH);
    
    ret = connect(s_fd_monitor, (struct sockaddr *)&addr, sizeof(addr)); 
    if(ret < 0)
    {
        upil_error("connect failed: %d", errno);
        return -1;
    } 

    ret = fcntl(s_fd_monitor, F_SETFL, O_NONBLOCK);
    if (ret < 0)
    {
        upil_warn("Error setting O_NONBLOCK errno:%d", errno);
    } 

    return 0;
}

/*
 * 断开与UPIL的 monitor socket 的连接
 * 参数:
 * 返回值:
 *     成功返回 0
 *     失败返回 -1
 */
int upil_monitor_disconnect()
{
    upil_info("Disconnect from upil monitor\n");

    if (s_fd_wakeup >= 0)
    {
        write(s_fd_wakeup, ".", 1);
    }

    s_fd_wakeup = -1;
    
	if (s_fd_monitor >= 0)
		close(s_fd_monitor);

    s_fd_monitor = -1;
    
    return 0;
}

/*
 * 接收 monitor socket 送来的数据
 * 参数:
 * 返回值:
 *     成功返回 读取到的字节数
 *     失败返回 -1
 */
int upil_monitor_recv(char *buf, size_t buf_size)
{
    fd_set rfds;
    int pair[2];
    int nfds = 0;
    int n=0;
    int ret = -1;

    if (pipe(pair) < 0) {
        upil_error("Error in pipe() errno:%d", errno);
        return -1;
    }

    fcntl(pair[0], F_SETFL, O_NONBLOCK);
    fcntl(pair[1], F_SETFL, O_NONBLOCK);

    s_fd_wakeup = pair[0];

    FD_ZERO(&rfds);
    FD_SET(s_fd_monitor, &rfds);
    nfds = s_fd_monitor+1;
    FD_SET(pair[1], &rfds);
    if (pair[1]>=nfds)
        nfds = pair[1]+1;

    n = select(nfds, &rfds, NULL, NULL, NULL);

    if (FD_ISSET(s_fd_monitor, &rfds))
    {
        ret = recv(s_fd_monitor, buf, buf_size-1, 0);

        if (ret <= 0)
            ret = -1;

        buf[ret] = '\0';
        upil_info("recv: %s\n", buf);
    }

    close(pair[0]);
    close(pair[1]);
    s_fd_wakeup = 0;
    
    return ret;
}

int upil_connect()
{
    int ret = 0;
    ret = upil_ctrl_connect();
    if (ret<0)
        return -1;
    
    ret = upil_monitor_connect();
    if (ret<0)
    {
        upil_ctrl_disconnect();
        return -2;
    }

    return 0;
}

void upil_disconnect()
{
    upil_ctrl_disconnect();
    upil_monitor_disconnect();
}

/*
    设备出现，返回1
    设备未出现，返回0
 */
int upil_is_device_exist()
{
    int state;
    if (upil_ctrl_get_state(&state)<0
        || state == UPIL_SM_OFFLINE)
        return 0;

    return 1;
}

/*
    设备已经初始化，返回1
    设备未初始化，返回0
 */
int upil_is_device_ready()
{
    int state;
    if (upil_ctrl_get_state(&state)<0
        || state < UPIL_SM_IDLE)
        return 0;

    return 1;
}

int upil_ril_write(int fd, const char* buffer, size_t buf_size)
{
    if (!strncmp(buffer, "ATD", 3))
    {
        upil_ctrl_dial(buffer+3);
    }
    else if (!strncmp(buffer, "ATH", 3))
    {
    }

    return 0;
}

int upil_ril_read(int fd, char* buffer, size_t buf_size)
{
    int rb = upil_monitor_recv(buffer, buf_size);

    if (rb<=0) return rb;

    if (!strncmp(buffer, "RING", 4))
    {
    }
    
    return upil_monitor_recv(buffer, buf_size);
}

