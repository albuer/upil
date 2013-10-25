#ifndef __UPIL_H__
#define __UPIL_H__

#include <upil_event/upil_event_wrapper.h>
#include <state_machine/state_machine.h>

#define UPIL_VERSION        "0.99"

#define REPLY_SUCCESS       "[SUCCESS]"
#define REPLY_SUCCESS_LEN   strlen(REPLY_SUCCESS)
#define REPLY_FAILURE       "[FAILURE]"
#define REPLY_FAILURE_LEN   strlen(REPLY_FAILURE)

#define UPIL_CTRL_GET_VER       "GET_VERSION"
#define UPIL_CTRL_GET_VER_LEN   strlen(UPIL_CTRL_GET_VER)
#define UPIL_CTRL_DIAL          "DIAL"
#define UPIL_CTRL_DIAL_LEN   strlen(UPIL_CTRL_DIAL)
#define UPIL_CTRL_HANGUP          "HANGUP"
#define UPIL_CTRL_HANGUP_LEN   strlen(UPIL_CTRL_HANGUP)
#define UPIL_CTRL_GET_STATE       "GET_CURR_STATE"
#define UPIL_CTRL_GET_STATE_LEN   strlen(UPIL_CTRL_GET_VER)

struct upil{
    uew_context event_loop;
    FSM* fsm;
    int device_fd;

    uchar version;
    uchar SN[8];
    uchar manufacturer[8];
    uchar hardware;
};

extern struct upil g_upil_s;

#endif

