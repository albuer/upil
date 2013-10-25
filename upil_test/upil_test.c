#include <upil_config.h>



#define LOG_TAG "UPIL_SM"
#include <utils/Log.h>


static void test0_enter(FSM* fsm)
{
    upil_dbg("========== in %s ===========\n", __FUNCTION__);
    // 进入dialing状态后，延时1s开始拨号
    sm_send_message_delay(&fsm->handler,
                            1,
                            NULL,
                            0,
                            3000);
}

static int test0_process(FSM* fsm, MSG* msg)
{
    upil_dbg("========== in %s ===========\n", __FUNCTION__);
    sm_send_message_delay(&fsm->handler,
                            123,
                            NULL,
                            0,
                            3000);
    upil_dbg("========== sm_transition_to ===========\n");
    sm_transition_to(fsm, 1);
    return 0;
}

static void test1_enter(FSM* fsm)
{
    upil_dbg("========== in %s ===========\n", __FUNCTION__);
    sm_remove_message_id(&fsm->handler.msg_queue, 123);
    upil_dbg("========== sm_send_message_delay ===========\n");
    sm_send_message_delay(&fsm->handler,
                            123,
                            NULL,
                            0,
                            2000);
}

static int test1_process(FSM* fsm, MSG* msg)
{
    upil_dbg("========== in %s ===========\n", __FUNCTION__);
    return 0;
}


BEGIN_FSM_STATE_TABLE(test_table)
    ADD_STATE(0, "test0", test0_enter, NULL, test0_process)
    ADD_STATE(1, "test1", test1_enter, NULL, test1_process)
END_FSM_STATE_TABLE(test_table)

static FSM* fsm = NULL;

int main_t()
{
    sm_init_pool();

    fsm = NULL;
    fsm = sm_obtain_fsm(&test_table[0], sizeof(test_table)/sizeof(FSM_STATE));
    upil_dbg("%s: fsm=%p\n", __FUNCTION__, fsm);
    if (fsm)
    {
        sm_set_init_state(fsm, 0);
        strcpy(fsm->name, "test_fsm");
    }


    while(1);
    return 0;
}

