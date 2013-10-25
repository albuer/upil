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

#ifndef __UPIL_REQUEST_H__
#define __UPIL_REQUEST_H__


    /*  常量名(SendCmd 第一个参数)    值 说明                    参数    */
typedef enum UPILReq{
    UPIL_CTL_BEGIN=0,
	UPIL_CTL_HANDFREE_ON=UPIL_CTL_BEGIN,         /* 0  免提摘机        NULL                     */
	UPIL_CTL_HANDFREE_OFF,          /* 1  免提挂机        NULL                     */
	UPIL_CTL_RECORD_ON,             /* 2  本地录音-开始   NULL                     */
	UPIL_CTL_RECORD_OFF,            /* 3  本地录音-结束   NULL                     */
	UPIL_CTL_R_FUNC,                /* 4  闪断/转接       n毫秒*10/78, 1字节       */
	UPIL_CTL_RECORD_PLAY_ON,        /* 5  喇叭-开         NULL                     */
	UPIL_CTL_RECORD_PLAY_OFF,       /* 6  喇叭-关         NULL                     */
	UPIL_CTL_RECORD_LINE_PLAY_ON,   /* 7  留言(线上)-开始 NULL                     */
	UPIL_CTL_RECORD_LINE_PLAY_OFF,  /* 8  留言(线上)-结束 NULL                     */
	UPIL_CTL_USB_ON,                /* 9  USB 激活/保活   NULL                     */
	UPIL_CTL_RINGER_OFF,            /* 10 话机自带振铃-关 NULL                     */
	UPIL_CTL_USB_VOIP_ON,           /* 11 VOIP-开始       NULL                     */
	UPIL_CTL_USB_VOIP_OFF,          /* 12 VOIP-结束       NULL                     */
	UPIL_ASK_PHONE_STATE,           /* 13 查询设备状态    NULL                     */
	UPIL_ASK_DTMF_TX,               /* 14 请求 DTMF 拨号  1~n 位号码, ASCII 值     */
	UPIL_ASK_RECORD_UP,             /* 15 取新来去电记录  NULL                     */
	UPIL_ASK_GET_VERSION,           /* 16 取版本信息      NULL                     */
	UPIL_ASK_AUTHORIZATION,         /* 17 (未用)                                   */
	UPIL_ASK_READ_MEMORY,           /* 18 读取内存        NULL                     */
	UPIL_CTL_RINGER_ON,             /* 19 话机自带振铃-开 NULL                     */
	UPIL_CTL_SET_TIME,              /* 20 设置话机的时间  年月日时分秒 6 字节      */
	UPIL_CTL_SPEECH_RECORDING_ON,   /* 21 通话录音-开始   NULL                     */
	UPIL_CTL_SPEECH_RECORDING_OFF,  /* 22 通话录音-结束   NULL                     */
	UPIL_ASK_BUSY_TONE,             /* 23 启动忙音检测    NULL                     */
	UPIL_CTL_SET_FLASH_TIME,        /* 24 设置 F 时间     n毫秒*10/78, 1字节       */
	UPIL_CTL_SET_MEMORY_NUMBER,          /* 25 设置 M 键对应号 ?                        */
	UPIL_ASK_READ_MANUFACTURER_INF, /* 26 读取厂家代码    NULL                     */
	UPIL_RESERVED_CMD,
	UPIL_ASK_READ_SN,               /* 28 读取序列号      NULL                     */
	UPIL_CTL_WRITE_SN,              /* 29 写入序列号      n 字节序列号, 非 ASCII   */
	UPIL_ASK_HARDWARE_INF,          /* 30 读取硬件类型    NULL                     */
	UPIL_CTL_VOIP_RINGER,           /* 31 (未用)                                   */
	UPIL_CTL_KEYBOARD_MAP,          /* 32 (未用)          ?                        */
	UPIL_CTL_DISP_ICON,             /* 33 (未用)          ?                        */
	UPIL_CTL_PHONE_VOLUME,          /* 34 (未用)          ?                        */
	UPIL_CTL_PHONE_UNMUTE,          /* 35 解除静音(留言)  NULL                     */
	UPIL_CTL_EEPROM_INIT,           /* 36 (未用)          ?                        */
	UPIL_CTL_RECORD_LIGHT_ON,       /* 37 留言录音灯-开   NULL                     */
	UPIL_CTL_RECORD_LIGHT_OFF,      /* 38 留言录音灯-关   NULL                     */
	UPIL_CTL_INI_SET,               /* 39 (未用)          ?                        */
	UPIL_CTL_BBK_VOIP_ON,           /* 40 (未用)          NULL                     */
	UPIL_CTL_BBK_VOIP_OFF,          /* 41 (未用)          NULL                     */

    UPIL_CTL_RECORD_START,
    UPIL_CTL_RECORD_STOP,
    
	UPIL_CTL_MAX,
} UPILReq;

#define UPIL_CTL_STRING_TABLE {       \
    "UPIL_CTL_HANDFREE_ON",           \
	"UPIL_CTL_HANDFREE_OFF",          \
	"UPIL_CTL_RECORD_ON",             \
	"UPIL_CTL_RECORD_OFF",            \
	"UPIL_CTL_R_FUNC",                \
	"UPIL_CTL_RECORD_PLAY_ON",        \
	"UPIL_CTL_RECORD_PLAY_OFF",       \
	"UPIL_CTL_RECORD_LINE_PLAY_ON",   \
	"UPIL_CTL_RECORD_LINE_PLAY_OFF",  \
	"UPIL_CTL_USB_ON",                \
	"UPIL_CTL_RINGER_OFF",            \
	"UPIL_CTL_USB_VOIP_ON",           \
	"UPIL_CTL_USB_VOIP_OFF",          \
	"UPIL_ASK_PHONE_STATE",           \
	"UPIL_ASK_DTMF_TX",               \
	"UPIL_ASK_RECORD_UP",             \
	"UPIL_ASK_GET_VERSION",           \
	"UPIL_ASK_AUTHORIZATION",         \
	"UPIL_ASK_READ_MEMORY",           \
	"UPIL_CTL_RINGER_ON",             \
	"UPIL_CTL_SET_TIME",              \
	"UPIL_CTL_SPEECH_RECORDING_ON",   \
	"UPIL_CTL_SPEECH_RECORDING_OFF",  \
	"UPIL_ASK_BUSY_TONE",             \
	"UPIL_CTL_SET_FLASH_TIME",        \
	"UPIL_CTL_SET_MEMORY_NUMBER",     \
	"UPIL_ASK_READ_MANUFACTURER_INF", \
	"UPIL_RESERVED_CMD",              \
	"UPIL_ASK_READ_SN",               \
	"UPIL_CTL_WRITE_SN",              \
	"UPIL_ASK_HARDWARE_INF",          \
	"UPIL_CTL_VOIP_RINGER",           \
	"UPIL_CTL_KEYBOARD_MAP",          \
	"UPIL_CTL_DISP_ICON",             \
	"UPIL_CTL_PHONE_VOLUME",          \
	"UPIL_CTL_PHONE_UNMUTE",          \
	"UPIL_CTL_EEPROM_INIT",           \
	"UPIL_CTL_RECORD_LIGHT_ON",       \
	"UPIL_CTL_RECORD_LIGHT_OFF",      \
	"UPIL_CTL_INI_SET",               \
	"UPIL_CTL_BBK_VOIP_ON",           \
	"UPIL_CTL_BBK_VOIP_OFF",          \
}

//  常量名(wParam)           值 说明          数据
typedef enum UPILEvent
{
    UPIL_EVT_BEGIN=UPIL_CTL_MAX,
	UPIL_EVT_RESERVE=UPIL_EVT_BEGIN,          /* 0  保留消息字    -                               */
	UPIL_EVT_USB_ATTACH,       /* 1  USB发现设备   -                               */
	UPIL_EVT_USB_DETACH,       /* 2  USB设备移出   -                               */
	UPIL_EVT_PC_HOOK_ON,       /* 3  (未用)                                        */
	UPIL_EVT_PC_HOOK_OFF,      /* 4  (未用)                                        */
	UPIL_EVT_HAND_HOOK_ON,     /* 5  手柄摘机      -                               */
	UPIL_EVT_HAND_HOOK_OFF,    /* 6  手柄挂机      -                               */
	UPIL_EVT_HFREE_HOOK_ON,    /* 7  免提摘机      -                               */
	UPIL_EVT_HFREE_HOOK_OFF,   /* 8  免提挂机      -                               */
	UPIL_EVT_PARALL_HOOK_ON,   /* 9  并机摘机      -                               */
	UPIL_EVT_PARALL_HOOK_OFF,  /* 10 并机挂机      -                               */
	UPIL_EVT_RING,             /* 11 来电振铃      -                               */
	UPIL_EVT_KEY_DOWN,         /* 12 设备按键      lParam=enum UsbPhoneKey         */
	UPIL_EVT_PHONE_STATE,      /* 13 设备状态查询  lParam=enum UsbPhoneState       */
	UPIL_EVT_CALL_ID,          /* 14 来电号码      ReadCallId() 读取               */
	UPIL_EVT_DIAL_END,         /* 15 拨号完成      -                               */
	UPIL_EVT_SEC_DTMF_RX,      /* 16 对方按键      lParam=DTMF 值                  */
	UPIL_EVT_RESERVED_1,
	UPIL_EVT_RESERVED_2,
	UPIL_EVT_RESERVED_3,
	UPIL_EVT_RESERVED_4,
	UPIL_EVT_RESERVED_5,
	UPIL_EVT_BUSY_ON,          /* 22 检测到忙音    -                               */
	UPIL_EVT_RESERVED_6,
	UPIL_EVT_NEW_CID_TX,       /* 24 未接来电上传  ReadCallId() 读取               */
	UPIL_EVT_OLD_CID_TX,       /* 25 已接来电上传  ReadCallId() 读取               */
	UPIL_EVT_DIALED_TX,        /* 26 拨出号码上传  ReadCallId() 读取               */
	UPIL_EVT_HISTORY_FINISHED, /* 27 记录上传完成  -                               */
	UPIL_EVT_VERSION,          /* 28 版本号        lParam=version                  */
	UPIL_EVT_RESTART,          /* 29 硬件启动/重启 -                               */
	UPIL_EVT_REDIAL,           /* 30 本机拨出 DTMF lParam=KEY_0 ~ P                */
	UPIL_EVT_BAT_CRITICAL,     /* 31 电池低电      -                               */
	UPIL_EVT_VOIP_HOOK_OFF,    /* 32 (未用)                                        */
	UPIL_EVT_VOIP_HOOK_ON,     /* 33 (未用)                                        */
	UPIL_EVT_PARAL_DTMF,       /* 34 并机拨出 DTMF lParam=DTMF 值                  */
	UPIL_MSG_VENDOR,           /* 35 厂家代码      ReadCallId() 读取               */
	UPIL_MSG_SERIAL,           /* 36 序列号        ReadCallId() 读取               */
	UPIL_MSG_HW_TYPE,          /* 37 硬件类型      lParam=UsbPhoneType             */
	UPIL_EVT_VOLUM_CHANGE,     /* 38 (未用)                                        */

    UPIL_EVT_ACK,
    UPIL_EVT_SYNC_TIMEOUT,
    UPIL_EVT_DIAL_END_CHECK,
    UPIL_EVT_RING_TIMEOUT,
    UPIL_EVT_RECORD_START,
    UPIL_EVT_RECORD_END,
    
	UPIL_EVT_MAX,

    UPIL_EVT_SM_TRANSITION = 0xEE,
	UPIL_EVT_UNKOWN = 0xFF,
} UPILEvent;

#define UPIL_EVENT_STRING_TABLE { \
    "UPIL_EVT_RESERVE",          \
    "UPIL_EVT_USB_ATTACH",       \
    "UPIL_EVT_USB_DETACH",       \
    "UPIL_EVT_PC_HOOK_ON",       \
    "UPIL_EVT_PC_HOOK_OFF",      \
    "UPIL_EVT_HAND_HOOK_ON",     \
    "UPIL_EVT_HAND_HOOK_OFF",    \
    "UPIL_EVT_HFREE_HOOK_ON",    \
    "UPIL_EVT_HFREE_HOOK_OFF",   \
    "UPIL_EVT_PARALL_HOOK_ON",   \
    "UPIL_EVT_PARALL_HOOK_OFF",  \
    "UPIL_EVT_RING",             \
    "UPIL_EVT_KEY_DOWN",         \
    "UPIL_EVT_PHONE_STATE",      \
    "UPIL_EVT_CALL_ID",          \
    "UPIL_EVT_DIAL_END",         \
    "UPIL_EVT_SEC_DTMF_RX",      \
    "UPIL_EVT_RESERVED_1",       \
    "UPIL_EVT_RESERVED_2",       \
    "UPIL_EVT_RESERVED_3",       \
    "UPIL_EVT_RESERVED_4",       \
    "UPIL_EVT_RESERVED_5",       \
    "UPIL_EVT_BUSY_ON",          \
    "UPIL_EVT_RESERVED_6",       \
    "UPIL_EVT_NEW_CID_TX",       \
    "UPIL_EVT_OLD_CID_TX",       \
    "UPIL_EVT_DIALED_TX",        \
    "UPIL_EVT_HISTORY_FINISHED", \
    "UPIL_EVT_VERSION",          \
    "UPIL_EVT_RESTART",          \
    "UPIL_EVT_REDIAL",           \
    "UPIL_EVT_BAT_CRITICAL",     \
    "UPIL_EVT_VOIP_HOOK_OFF",    \
    "UPIL_EVT_VOIP_HOOK_ON",     \
    "UPIL_EVT_PARAL_DTMF",       \
    "UPIL_MSG_VENDOR",           \
    "UPIL_MSG_SERIAL",           \
    "UPIL_MSG_HW_TYPE",          \
    "UPIL_EVT_VOLUM_CHANGE",     \
    "UPIL_EVT_ACK",              \
    "UPIL_EVT_SYNC_TIMEOUT",     \
    "UPIL_EVT_DIAL_END_CHECK",   \
    "UPIL_EVT_RING_TIMEOUT",     \
    "UPIL_EVT_START_MEMO",     \
}

#define UPIL_STATE_OFFLINE     0x160
#define UPIL_STATE_SYNCING     0x161
#define UPIL_STATE_IDLE        0x162
#define UPIL_STATE_DIALING     0x163
#define UPIL_STATE_INCOMING    0x164
#define UPIL_STATE_CONVERSATION     0x165
#define UPIL_STATE_MEMO         0x166

#endif

