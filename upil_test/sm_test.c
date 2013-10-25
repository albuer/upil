#include <upil_config.h>

#define LOG_TAG "UPIL_SM_TEST"
#define LOG_NDEBUG 0 // 0 - Enable LOGV, 1 - Disable LOGV
#include <utils/Log.h>

FSM* fsm = NULL;

static void state0_enter(FSM* fsm)
{
    sm_remove_message_id(fsm, 1);
    
    sm_send_message_delay(&fsm->handler,
                            0,
                            NULL,
                            0,
                            1000);
    sm_send_message_delay(&fsm->handler,
                            1,
                            NULL,
                            0,
                            3000);
}

static int state0_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case 0:
        sm_transition_to(fsm, 0);
        break;
    case 1:
        sm_transition_to(fsm, 1);
        sm_send_message_delay(&fsm->handler,
                                0,
                                NULL,
                                0,
                                3000);
        break;
    case 2:
        sm_transition_to(fsm, 2);
    default:
        break;
    }

    return HANDLED;
}

static void state1_enter(FSM* fsm)
{
}

static int state1_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case 0:
        sm_transition_to(fsm, 0);
        break;
    case 1:
        sm_transition_to(fsm, 1);
        break;
    case 2:
        sm_transition_to(fsm, 2);
    default:
        break;
    }

    return HANDLED;
}

static void state2_enter(FSM* fsm)
{
}

static int state2_process(FSM* fsm, MSG* msg)
{
    switch (msg->what)
    {
    case 0:
        sm_transition_to(fsm, 0);
        break;
    case 1:
        sm_transition_to(fsm, 1);
        break;
    case 2:
        sm_transition_to(fsm, 2);
    default:
        break;
    }

    return HANDLED;
}

BEGIN_FSM_STATE_TABLE(test_table)
    ADD_STATE(0, "state_0", state0_enter, NULL, state0_process)
    ADD_STATE(1, "state_1", state1_enter, NULL, state1_process)
    ADD_STATE(2, "state_2", state2_enter, NULL, state2_process)
END_FSM_STATE_TABLE(test_table)


int main()
{
    fsm = sm_obtain_fsm(&test_table[0], sizeof(test_table)/sizeof(FSM_STATE));
    int count = 0;
    if (fsm)
    {
        sm_set_init_state(fsm, 0);
        strcpy(fsm->name, "TEST_SM");
        upil_dump_fsm(fsm);
    }
    else
        LOGE("*** can not obtain FSM");

    
    while(1) {
        sm_send_message_delay(&fsm->handler,
                                count%3,
                                NULL,
                                0,
                                (count%8)*400);
        sleep(1);
        ++count;
    }
    
    return 0;
}

