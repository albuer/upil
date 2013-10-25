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

#ifndef __UPIL_EVENT_WRAPPER_H__
#define __UPIL_EVENT_WRAPPER_H__

#include "upil_event.h"

typedef struct uew_context_t{
    upile_context upile;
    int fdWakeupRead;
    int fdWakeupWrite;
    struct upil_event wakeupfd_event;
    pthread_t tid_dispatch;
}uew_context;

// 添加一个定时器到监视列表中并激活
void uew_timer_add_wakeup(uew_context* context, struct upil_event *ev, int msec);

// 添加一个事件到监视列表中并激活
void uew_event_add_wakeup(uew_context* context, struct upil_event *ev);

// 从监视列表中删除一个事件
void uew_event_del(uew_context* context, struct upil_event * ev, int is_timer);

// 初始化一个事件
void uew_event_set(struct upil_event * ev, int fd, int persist, upil_event_cb func, void * param);

// 启动事件循环
int uew_startEventLoop(uew_context* context);

#endif

