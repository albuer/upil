/*
**
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

#ifndef __UPIL_EVENT_H__
#define __UPIL_EVENT_H__

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

// Max number of fd's we watch at any one time.  Increase if necessary.
#define MAX_FD_EVENTS 16

typedef int (*upil_event_cb)(int fd, short events, void *userdata);

struct upil_event {
    struct upil_event *next;
    struct upil_event *prev;

    int fd;
    int index;
    int persist;
    struct timeval timeout;
    upil_event_cb func;
    void *param;
};

typedef struct upile_context_t{
    fd_set readFds;
    int nfds;
    
    struct upil_event * watch_table[MAX_FD_EVENTS];
    struct upil_event timer_list;
    struct upil_event pending_list;
    
    pthread_mutex_t listMutex;
}upile_context;


// Initialize internal data structs
void upil_event_init(upile_context* context);

// Initialize an event
void upil_event_set(struct upil_event * ev, int fd, int persist, upil_event_cb func, void * param);

// Add event to watch list
void upil_event_add(upile_context* context, struct upil_event * ev);

// Add timer event
void upil_timer_add(upile_context* context, struct upil_event * ev, struct timeval * tv);

// Remove event from timer list
void upil_timer_del(upile_context* context, struct upil_event * ev);

// Remove event from watch list
void upil_event_del(upile_context* context, struct upil_event * ev);

// Event loop
void upil_event_loop(upile_context* context);

#endif

