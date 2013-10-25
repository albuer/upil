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

#include <stdio.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <fcntl.h>

#include <upil_config.h>
#include "upil.h"

#define LOG_TAG "UPIL"
#include <utils/Log.h>

#define UPIL_DEV_PATH   "/dev/hidraw0"

static int s_dev_fd = 0;

static struct upil_event s_device_event;

struct upil g_upil_s;

extern int setup_cmd_processor(void* event_loop);
extern int upil_ril_setup(void* event_loop);

/** returns 1 if line starts with prefix, 0 if it does not */
int strStartsWith(const char *line, const char *prefix)
{
    for ( ; *line != '\0' && *prefix != '\0' ; line++, prefix++) {
        if (*line != *prefix) {
            return 0;
        }
    }

    return *prefix == '\0';
}

/*
 * 打开设备，并将得到的s_dev_fd加入Event Loop
 * 
 * 参数:
 *
 * 返回值:
 *     正常返回 0
 *     出现异常，则返回 -1
 */
static int setup_device(void)
{
    int ret;

    // add device monitor to event loop
    upil_dbg("Open file: %s\n", UPIL_DEV_PATH);
    s_dev_fd = open(UPIL_DEV_PATH, O_RDWR);
    if (s_dev_fd<0)
    {
        upil_error("Can not open file: %s\n", strerror(errno));
        return -1;
    }
    g_upil_s.device_fd = s_dev_fd;

    uew_event_set(&s_device_event, s_dev_fd, 1, tpp_recv_event_cb, &s_device_event);
    uew_event_add_wakeup(&g_upil_s.event_loop, &s_device_event);

    // 发送 USB_ATTACH 的消息给状态机
    upil_sm_usb_attach(g_upil_s.fsm);

    return 0;
}

// 等待直到设备出现
static int wait_device()
{
    for (;;)
    {
        if (access(UPIL_DEV_PATH, 0) == 0)
            break;

        upil_dbg("Wait device\n");
        sleep(1);
    }
    sleep(1);

    return 0;
}

void usage(char *argv0)
{
    upil_info("Usage %s:\n", argv0);
	upil_info("\t<-d> to output debug info\n");
	upil_info("\t<-t> to print timestamp\n");
}

static void upil_handle_signal(int sig)
{
    upil_info("--- END UPIL ---");
    // 做清除动作
    exit(0);
}

// upil -dd -t
int main(int argc, char** argv)
{
    int ch;

    upil_info("UPIL version: %s", UPIL_VERSION);
    
    while ((ch = getopt(argc,argv,"dt"))!= -1)
    {
        switch (ch) {
        case 'd':// debug
            --upil_debug_level;
            break;
        case 't':// print timestamp
            upil_debug_ts = 1;
            break;
        default:
            usage(argv[0]);
            exit(-1);
        }
    }

    signal(SIGINT, upil_handle_signal);
    signal(SIGTERM, upil_handle_signal);

    upil_info("Init state machine pool\n");
    sm_init_pool();

    upil_info("Start UPIL state machine\n");
    g_upil_s.fsm = upil_sm_start("MainSM");
    if (g_upil_s.fsm == NULL)
    {
        upil_error("Start state machine failed\n");
        exit(-1);
    }
    
    upil_info("Start event loop...\n");
    if (uew_startEventLoop(&g_upil_s.event_loop))
    {
        upil_error("Failed in start event loop\n");
        exit(-1);
    }

    // setup command processor
    upil_info("Setup upil ctrl...\n");
    setup_cmd_processor(&g_upil_s.event_loop);

    // setup event monitor
    upil_info("Setup upil monitor...\n");
    setup_event_monitor(&g_upil_s.event_loop);

    upil_info("Wait device...\n");
    wait_device();
    
    upil_info("Add device to event loop\n");
    if (setup_device())
    {
        upil_error("Setup device failed!\n");
        exit(-1);
    }

    upil_info("Setup ril helper...\n");
    upil_ril_setup(&g_upil_s.event_loop);

    while(1) {
        // sleep(UINT32_MAX) seems to return immediately on bionic
        sleep(0x00ffffff);
    }
    
    return 0;
}

