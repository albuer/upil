#ifndef __UPIL_SM_H__
#define __UPIL_SM_H__

#include <state_machine/state_machine.h>

typedef enum UPIL_SM{
    UPIL_SM_OFFLINE=0,
    UPIL_SM_SYNCING,
    UPIL_SM_IDLE,
    UPIL_SM_DIALING,
    UPIL_SM_INCOMING,
    UPIL_SM_CONVERSATION,
    UPIL_SM_MEMO,
}UPIL_SM;

#define UPIL_CTL_DIAL           0x200

FSM* upil_sm_start(const char* name);
int upil_sm_usb_attach(FSM* fsm);
int upil_sm_get_version(FSM* fsm, char** reply);
int upil_sm_send_msg(FSM* fsm, int event, void* data, int data_size);
int upil_sm_start_dail(FSM* fsm, const char* number);
int upil_sm_hangup(FSM* fsm);

#endif

