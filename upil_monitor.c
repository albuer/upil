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
    创建 socket: /dev/socket/upil.evt，然后等待外部程序的连接
    当外部程序连接到该socket之后，外部程序就可以通过该socket得到upil的状态
 */

#include <stdio.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <fcntl.h>

#include <upil_config.h>
#include "upil.h"

static struct upil_event s_listen_event;
static struct upil_event s_monitor_event;

static uew_context* s_event_loop=NULL;
static int s_fdMonitor;

extern void upil_ril_unsolicited(int msg);

static int event_monitor_cb(int fd, short flags, void *param)
{
    int ret = 0;
    char buf[16];
    ENTER_LOG();

    if(recv(fd, buf, 1, 0) > 0)
    {
        upil_warn("Don't write to this socket.");
    }
    else
    {
        upil_warn("Event socket disconnected\n");
        close(fd);

        uew_event_del(s_event_loop, &s_monitor_event, 0);

        /* start listening for new connections again */
        uew_event_add_wakeup(s_event_loop, &s_listen_event);
    }

    LEAVE_LOG();

    return 0;
}

static int listenCallback (int fd, short flags, void *param) {
    int ret;

    ENTER_LOG();
    
    s_fdMonitor = accept(fd, NULL, NULL);

    if (s_fdMonitor < 0 ) {
        upil_error("Error on accept() errno:%d\n", errno);
        /* start listening for new connections again */
        uew_event_add_wakeup(&g_upil_s.event_loop, &s_listen_event);
        LEAVE_LOG();
        return -1;
    }

    ret = fcntl(s_fdMonitor, F_SETFL, O_NONBLOCK);

    if (ret < 0) {
        upil_error("Error setting O_NONBLOCK errno:%d\n", errno);
    }

    uew_event_set(&s_monitor_event, s_fdMonitor, 1, event_monitor_cb, NULL);

    uew_event_add_wakeup(s_event_loop, &s_monitor_event);

    LEAVE_LOG();

    return 0;
}

// Send event/msg to monitor sockets
int sendto_monitor(int msg, uchar* data, size_t data_size)
{
    char event[512];

    switch (msg)
    {
    case UPIL_STATE_OFFLINE:
        sprintf(event, "STATE OFFLINE");
        break;
        
    case UPIL_STATE_SYNCING:
        sprintf(event, "STATE SYNCING");
        break;
        
    case UPIL_STATE_IDLE:
        sprintf(event, "STATE IDLE");
        break;
        
    case UPIL_STATE_DIALING:
        sprintf(event, "STATE DIALING");
        break;
        
    case UPIL_STATE_INCOMING:
        sprintf(event, "STATE INCOMING");
        break;
        
    case UPIL_STATE_CONVERSATION:
        sprintf(event, "STATE CONVERSATION");
        break;
        
    case UPIL_STATE_MEMO:
        sprintf(event, "STATE MEMO");
        break;
            
    case UPIL_EVT_RING:
        sprintf(event, "RING");
        break;
        
    case UPIL_EVT_CALL_ID:
        sprintf(event, "CID %s", (char*)data);
        break;

    case UPIL_EVT_USB_ATTACH:
        sprintf(event, "INITIALIZE");
        break;

    default:
        upil_warn("unknown event!");;
        return 0;
    }
    
    send(s_fdMonitor, event, strlen(event), 0);

    upil_ril_unsolicited(msg);
    
    return 0;//
}

/*
 * 安装事件监视器，把程序的内部事件、信息等报给外部程序
 * 参数:
 *     event_loop   - command socket 将添加到该event loop中
 * 返回值:
 *     成功返回 0
 *     出现异常，则返回 -1
 */
int setup_event_monitor(void* event_loop)
{
    int fd;
    ENTER_LOG();

    if (event_loop==NULL)
    {
        upil_error("event loop can not be empty!");
        LEAVE_LOG();
        return -1;
    }

    unlink(UPIL_SOCKET_EVT_PATH);
    
    // add listen to event loop for event channel connect
    fd = socket_local_server (UPIL_SOCKET_EVT, 1, SOCK_STREAM);

    if (fd < 0) {
        upil_error("Unable to bind socket: %s\n", strerror(errno));
        LEAVE_LOG();
        return -1;
    }
    s_event_loop = (uew_context*)event_loop;
    
    uew_event_set (&s_listen_event, fd, 0, listenCallback, NULL);
    
    uew_event_add_wakeup(s_event_loop, &s_listen_event);

    LEAVE_LOG();
    return 0;
}

