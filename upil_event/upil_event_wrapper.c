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

#include <alloca.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <upil_config.h>

#include <fcntl.h>
#include <cutils/sockets.h>
#include <cutils/record_stream.h>

#define LOG_TAG "UPIL_EVENT_WRAP"
#undef LOG_NDEBUG
#define LOG_NDEBUG 1 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>


#include "upil_event_wrapper.h"


static int s_started = 0;
static pthread_mutex_t s_startupMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_startupCond = PTHREAD_COND_INITIALIZER;

/**
 * A write on the wakeup fd is done just to pop us out of select()
 * We empty the buffer here and then upil_event will reset the timers on the
 * way back down
 */
static int processWakeupCallback(int fd, short flags, void *param) {
    char buff[16];
    int ret;

    /* empty our wakeup socket out */
    do {
        ret = read(fd, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));

    return 0;
}

static void triggerEvLoop(uew_context* context) {
    int ret;
    if (!pthread_equal(pthread_self(), context->tid_dispatch)) {
        /* trigger event loop to wakeup. No reason to do this,
         * if we're in the event loop thread */
         do {
            ret = write (context->fdWakeupWrite, " ", 1);
         } while (ret < 0 && errno == EINTR);
    }
}

static void * eventLoop(void *param) {
    int ret;
    int filedes[2];
    uew_context* context = (uew_context*)param;

    upil_event_init(&context->upile);

    pthread_mutex_lock(&s_startupMutex);

    s_started = 1;
    pthread_cond_broadcast(&s_startupCond);

    pthread_mutex_unlock(&s_startupMutex);

    ret = pipe(filedes);

    if (ret < 0) {
        upil_error("Error in pipe() errno:%d", errno);
        return NULL;
    }

    context->fdWakeupRead = filedes[0];
    context->fdWakeupWrite = filedes[1];

    fcntl(context->fdWakeupRead, F_SETFL, O_NONBLOCK);

    upil_event_set (&context->wakeupfd_event, context->fdWakeupRead, 1,
                processWakeupCallback, NULL);

    uew_event_add_wakeup (context, &context->wakeupfd_event);

    // Only returns on error
    upil_event_loop(&context->upile);
    upil_error("error in event_loop_base errno: %d", errno);
    
    kill(0, SIGKILL);

    return NULL;
}

/*
 * 启动 Event Loop, 用于监控设备文件
 * 参数:
 *     context 
 * 返回值:
 *     启动成功返回0，否则返回-1
 */
int uew_startEventLoop(uew_context* context) {
    int ret;
    pthread_attr_t attr;

    s_started = 0;
    pthread_mutex_lock(&s_startupMutex);

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&context->tid_dispatch, &attr, eventLoop, (void*)context);

    while (s_started == 0) {
        pthread_cond_wait(&s_startupCond, &s_startupMutex);
    }

    pthread_mutex_unlock(&s_startupMutex);

    if (ret < 0) {
        upil_error("Failed to create thread, errno:%d", errno);
        return -1;
    }

    return 0;
}

void uew_event_add_wakeup(uew_context* context, struct upil_event *ev)
{
    ENTER_LOG();

    upil_printf(MSG_MSGDUMP, "Add event(%p) to loop", ev);
    upil_event_add(&context->upile, ev);
    triggerEvLoop(context);

    LEAVE_LOG();
}

void uew_timer_add_wakeup(uew_context* context, struct upil_event *ev, int msec)
{
    struct timeval relativeTime;
    ENTER_LOG();

    upil_printf(MSG_MSGDUMP, "Add timer(%p) to loop", ev);

    relativeTime.tv_sec = msec/1000;
    relativeTime.tv_usec = (msec%1000)*1000;

    upil_timer_add(&context->upile, ev, &relativeTime);
    
    triggerEvLoop(context);

    LEAVE_LOG();
}

// Remove event from watch or timer list
void uew_event_del(uew_context* context, struct upil_event * ev, int is_timer)
{
    upil_printf(MSG_MSGDUMP, "delete %s(%p) from loop", is_timer?"timer":"event", ev);
    
    is_timer ? upil_timer_del(&context->upile, ev)
             : upil_event_del(&context->upile, ev);
}

// Initialize an event
// func指向回调函数，当回调函数的返回值 <0 时，将退出event loop
void uew_event_set(struct upil_event * ev, int fd, int persist, upil_event_cb func, void * param)
{
    upil_event_set(ev, fd, persist, func, param);
}

