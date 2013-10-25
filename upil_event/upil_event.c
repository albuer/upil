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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define LOG_TAG "UPIL_EVENT"
#include <utils/Log.h>

#include "upil_event.h"

#define MUTEX_ACQUIRE(listMutex) pthread_mutex_lock(&listMutex)
#define MUTEX_RELEASE(listMutex) pthread_mutex_unlock(&listMutex)
#define MUTEX_INIT(listMutex) pthread_mutex_init(&listMutex, NULL)
#define MUTEX_DESTROY(listMutex) pthread_mutex_destroy(&listMutex)

#ifndef timeradd
#define timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif

#ifndef timercmp
#define timercmp(a, b, op)               \
        ((a)->tv_sec == (b)->tv_sec      \
        ? (a)->tv_usec op (b)->tv_usec   \
        : (a)->tv_sec op (b)->tv_sec)
#endif

#ifndef timersub
#define timersub(a, b, res)                           \
    do {                                              \
        (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
        (res)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((res)->tv_usec < 0) {                     \
            (res)->tv_usec += 1000000;                \
            (res)->tv_sec -= 1;                       \
        }                                             \
    } while(0);
#endif

static void dump_event(struct upil_event * ev)
{
    if (MSG_EXCESSIVE<upil_debug_level)
        return;
    
    upil_printf(MSG_EXCESSIVE, "~~~~ Event %x ~~~~", (unsigned int)ev);
    upil_printf(MSG_EXCESSIVE, "     next    = %x", (unsigned int)ev->next);
    upil_printf(MSG_EXCESSIVE, "     prev    = %x", (unsigned int)ev->prev);
    upil_printf(MSG_EXCESSIVE, "     fd      = %d", ev->fd);
    upil_printf(MSG_EXCESSIVE, "     pers    = %d", ev->persist);
    upil_printf(MSG_EXCESSIVE, "     timeout = %ds + %dus", (int)ev->timeout.tv_sec, (int)ev->timeout.tv_usec);
    upil_printf(MSG_EXCESSIVE, "     func    = %x", (unsigned int)ev->func);
    upil_printf(MSG_EXCESSIVE, "     param   = %x", (unsigned int)ev->param);
    upil_printf(MSG_EXCESSIVE, "~~~~~~~~~~~~~~~~~~");
}

static void printReadies(upile_context* context, fd_set * rfds)
{
    int i=0;
    for (i = 0; (i < MAX_FD_EVENTS); i++) {
        struct upil_event * rev = context->watch_table[i];
        if (rev != NULL && FD_ISSET(rev->fd, rfds)) {
          upil_printf(MSG_EXCESSIVE, "DON: fd=%d is ready", rev->fd);
        }
    }
}

static void getNow(struct timeval * tv)
{
#ifdef HAVE_POSIX_CLOCKS
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec/1000;
#else
    gettimeofday(tv, NULL);
#endif
}

static void init_list(struct upil_event * list)
{
    memset(list, 0, sizeof(struct upil_event));
    list->next = list;
    list->prev = list;
    list->fd = -1;
}

static void addToList(struct upil_event * ev, struct upil_event * list)
{
    ev->next = list;
    ev->prev = list->prev;
    ev->prev->next = ev;
    list->prev = ev;
    dump_event(ev);
}

static void removeFromList(struct upil_event * ev)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ Removing event ~~~~");
    dump_event(ev);

    ev->next->prev = ev->prev;
    ev->prev->next = ev->next;
    ev->next = NULL;
    ev->prev = NULL;
}


static void removeWatch(upile_context* context, struct upil_event * ev, int index)
{
    context->watch_table[index] = NULL;
    ev->index = -1;

    FD_CLR(ev->fd, &context->readFds);

    if (ev->fd+1 == context->nfds) {
        int n = 0;
        int i = 0;

        for (i = 0; i < MAX_FD_EVENTS; i++) {
            struct upil_event * rev = context->watch_table[i];

            if ((rev != NULL) && (rev->fd > n)) {
                n = rev->fd;
            }
        }
        context->nfds = n + 1;
        upil_printf(MSG_EXCESSIVE, "~~~~ nfds = %d ~~~~", context->nfds);
    }
}

static void processTimeouts(upile_context* context)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ +processTimeouts ~~~~");
    MUTEX_ACQUIRE(context->listMutex);
    struct timeval now;
    struct upil_event * tev = context->timer_list.next;
    struct upil_event * next;

    getNow(&now);
    // walk list, see if now >= ev->timeout for any events

    upil_printf(MSG_EXCESSIVE, "~~~~ Looking for timers <= %ds + %dus ~~~~", (int)now.tv_sec, (int)now.tv_usec);
    while ((tev != &context->timer_list) && (timercmp(&now, &tev->timeout, >))) {
        // Timer expired
        upil_printf(MSG_EXCESSIVE, "~~~~ firing timer ~~~~");
        next = tev->next;
        removeFromList(tev);
        addToList(tev, &context->pending_list);
        tev = next;
    }
    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -processTimeouts ~~~~");
}

static void processReadReadies(upile_context* context, fd_set * rfds, int n)
{
    int i = 0;
    upil_printf(MSG_EXCESSIVE, "~~~~ +processReadReadies (%d) ~~~~", n);
    MUTEX_ACQUIRE(context->listMutex);

    for (i = 0; (i < MAX_FD_EVENTS) && (n > 0); i++) {
        struct upil_event * rev = context->watch_table[i];
        if (rev != NULL && FD_ISSET(rev->fd, rfds)) {
            addToList(rev, &context->pending_list);
            if (rev->persist == 0) {
                removeWatch(context, rev, i);
            }
            n--;
        }
    }

    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -processReadReadies (%d) ~~~~", n);
}

static int firePending(upile_context* context)
{
    int ret = 0;
    upil_printf(MSG_EXCESSIVE, "~~~~ +firePending ~~~~");
    struct upil_event * ev = context->pending_list.next;
    while (ev != &context->pending_list) {
        struct upil_event * next = ev->next;
        removeFromList(ev);
        ret = ev->func(ev->fd, 0, ev->param);
        if (ret<0) break;
        ev = next;
    }
    upil_printf(MSG_EXCESSIVE, "~~~~ -firePending ~~~~");

    return ret;
}

static int calcNextTimeout(upile_context* context, struct timeval * tv)
{
    struct upil_event * tev = context->timer_list.next;
    struct timeval now;

    getNow(&now);

    // Sorted list, so calc based on first node
    if (tev == &context->timer_list) {
        // no pending timers
        return -1;
    }

    upil_printf(MSG_EXCESSIVE, "~~~~ now = %ds + %dus ~~~~", (int)now.tv_sec, (int)now.tv_usec);
    upil_printf(MSG_EXCESSIVE, "~~~~ next = %ds + %dus ~~~~",
            (int)tev->timeout.tv_sec, (int)tev->timeout.tv_usec);
    if (timercmp(&tev->timeout, &now, >)) {
        timersub(&tev->timeout, &now, tv);
    } else {
        // timer already expired.
        tv->tv_sec = tv->tv_usec = 0;
    }
    return 0;
}

// Initialize internal data structs
void upil_event_init(upile_context* context)
{
    MUTEX_INIT(context->listMutex);

    FD_ZERO(&context->readFds);
    init_list(&context->timer_list);
    init_list(&context->pending_list);
    memset(context->watch_table, 0, sizeof(context->watch_table));
    context->nfds = 0;
}

// Initialize an event
void upil_event_set(struct upil_event * ev, int fd, int persist, upil_event_cb func, void * param)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ upil_event_set %x ~~~~", (unsigned int)ev);
    memset(ev, 0, sizeof(struct upil_event));
    ev->fd = fd;
    ev->index = -1;
    ev->persist = persist;
    ev->func = func;
    ev->param = param;
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

// Add event to watch list
void upil_event_add(upile_context* context, struct upil_event * ev)
{
    int i = 0;
    upil_printf(MSG_EXCESSIVE, "~~~~ +upil_event_add ~~~~");
    MUTEX_ACQUIRE(context->listMutex);
    for (i = 0; i < MAX_FD_EVENTS; i++) {
        if (context->watch_table[i] == NULL) {
            context->watch_table[i] = ev;
            ev->index = i;
            upil_printf(MSG_EXCESSIVE, "~~~~ added at %d ~~~~", i);
            dump_event(ev);
            FD_SET(ev->fd, &context->readFds);
            if (ev->fd >= context->nfds) context->nfds = ev->fd+1;
            upil_printf(MSG_EXCESSIVE, "~~~~ nfds = %d ~~~~", context->nfds);
            break;
        }
    }
    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -upil_event_add ~~~~");
}

// Add timer event
void upil_timer_add(upile_context* context, struct upil_event * ev, struct timeval * tv)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ +upil_timer_add ~~~~");
    MUTEX_ACQUIRE(context->listMutex);

    struct upil_event * list;
    if (tv != NULL) {
        // add to timer list
        list = context->timer_list.next;
        ev->fd = -1; // make sure fd is invalid

        struct timeval now;
        getNow(&now);
        timeradd(&now, tv, &ev->timeout);

        // keep list sorted
        while (timercmp(&list->timeout, &ev->timeout, < )
                && (list != &context->timer_list)) {
            list = list->next;
        }
        // list now points to the first event older than ev
        addToList(ev, list);
    }

    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -upil_timer_add ~~~~");
}

// Remove event from timer list
void upil_timer_del(upile_context* context, struct upil_event * ev)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ +upil_timer_del ~~~~");
    MUTEX_ACQUIRE(context->listMutex);

    removeFromList(ev);

    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -upil_timer_del ~~~~");
}

// Remove event from watch or timer list
void upil_event_del(upile_context* context, struct upil_event * ev)
{
    upil_printf(MSG_EXCESSIVE, "~~~~ +upil_event_del ~~~~");
    MUTEX_ACQUIRE(context->listMutex);

    if (ev->index < 0 || ev->index >= MAX_FD_EVENTS) {
        MUTEX_RELEASE(context->listMutex);
        return;
    }

    removeWatch(context, ev, ev->index);

    MUTEX_RELEASE(context->listMutex);
    upil_printf(MSG_EXCESSIVE, "~~~~ -upil_event_del ~~~~");
}

void upil_event_loop(upile_context* context)
{
    int n;
    fd_set rfds;
    struct timeval tv;
    struct timeval * ptv;
    int ret=0;

    upil_printf(MSG_EXCESSIVE, "Enter UPIL event loop");
    for (;;) {
        // make local copy of read fd_set
        memcpy(&rfds, &context->readFds, sizeof(fd_set));
        if (-1 == calcNextTimeout(context, &tv)) {
            // no pending timers; block indefinitely
            upil_printf(MSG_EXCESSIVE, "~~~~ no timers; blocking indefinitely ~~~~");
            ptv = NULL;
        } else {
            upil_printf(MSG_EXCESSIVE, "~~~~ blocking for %ds + %dus ~~~~", (int)tv.tv_sec, (int)tv.tv_usec);
            ptv = &tv;
        }
        printReadies(context, &rfds);
        n = select(context->nfds, &rfds, NULL, NULL, ptv);
        printReadies(context, &rfds);
        upil_printf(MSG_EXCESSIVE, "~~~~ %d events fired ~~~~", n);
        if (n < 0) {
            if (errno == EINTR) continue;

            upil_error("upil_event: select error (%d)", errno);

            ret = -1;
            break;
        }

        // Check for timeouts
        processTimeouts(context);
        // Check for read-ready
        processReadReadies(context, &rfds, n);
        // Fire away
        ret = firePending(context);
        if (ret<0) break;
    }
}
