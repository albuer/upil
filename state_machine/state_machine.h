#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include "handler.h"

#define HANDLED         1
#define NOT_HANDLED     0

typedef int FSM_EVENT_ID;
typedef int FSM_MSG_ID;
typedef int FSM_STATE_ID;


typedef struct fsm_t
{
    char name[64];
    struct fsm_state_t* table;
    int                 size;
    struct fsm_state_t* cur_state;
    HANDLER             handler;
    char last_incoming_num[128];
    char last_outgoing_num[128];
}FSM;

typedef void (*FSM_FUNC)(FSM*);
typedef int (*FSM_PROCESS_FUNC)(FSM*, MSG*);

typedef struct fsm_state_t
{
    FSM_STATE_ID        id;
    const char *        name;
    FSM_FUNC            enter_func;
    FSM_FUNC            exit_func;
    FSM_PROCESS_FUNC    process_func;
}FSM_STATE;

typedef FSM_STATE STATE_TABLE[];
#define BEGIN_FSM_STATE_TABLE(state_stable) static STATE_TABLE state_stable={
#define ADD_STATE(id,name,enter_func,exit_func,process_func) {id,name,enter_func,exit_func,process_func},
#define END_FSM_STATE_TABLE(state_stable) };


int sm_transition_to(FSM* fsm, FSM_STATE_ID id);
int sm_process_msg(FSM* fsm, MSG* msg);
FSM* sm_obtain_fsm(FSM_STATE* table, int size);
int sm_set_init_state(FSM* fsm, FSM_STATE_ID id);
void sm_init_pool();

//int sm_send_message(int what, void* data, int data_size);
//int sm_send_message_delay(int what, void* data, int data_size, uint msec);

int sm_send_message_delay(HANDLER* handler, int what, void* data, int data_size, int msec);
#define sm_obtain_message(h,w,d,s)      obtain_message(h,w,d,s)
#define sm_send_message(h,w,d,s)        sm_send_message_delay(h,w,d,s,0)
//#define sm_remove_message_id(queue,id)    remove_message_id(queue,id)
void sm_remove_message_id(FSM* fsm, int what);


#endif

