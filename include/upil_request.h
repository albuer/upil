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


    /*  ������(SendCmd ��һ������)    ֵ ˵��                    ����    */
typedef enum UPILReq{
    UPIL_CTL_BEGIN=0,
	UPIL_CTL_HANDFREE_ON=UPIL_CTL_BEGIN,         /* 0  ����ժ��        NULL                     */
	UPIL_CTL_HANDFREE_OFF,          /* 1  ����һ�        NULL                     */
	UPIL_CTL_RECORD_ON,             /* 2  ����¼��-��ʼ   NULL                     */
	UPIL_CTL_RECORD_OFF,            /* 3  ����¼��-����   NULL                     */
	UPIL_CTL_R_FUNC,                /* 4  ����/ת��       n����*10/78, 1�ֽ�       */
	UPIL_CTL_RECORD_PLAY_ON,        /* 5  ����-��         NULL                     */
	UPIL_CTL_RECORD_PLAY_OFF,       /* 6  ����-��         NULL                     */
	UPIL_CTL_RECORD_LINE_PLAY_ON,   /* 7  ����(����)-��ʼ NULL                     */
	UPIL_CTL_RECORD_LINE_PLAY_OFF,  /* 8  ����(����)-���� NULL                     */
	UPIL_CTL_USB_ON,                /* 9  USB ����/����   NULL                     */
	UPIL_CTL_RINGER_OFF,            /* 10 �����Դ�����-�� NULL                     */
	UPIL_CTL_USB_VOIP_ON,           /* 11 VOIP-��ʼ       NULL                     */
	UPIL_CTL_USB_VOIP_OFF,          /* 12 VOIP-����       NULL                     */
	UPIL_ASK_PHONE_STATE,           /* 13 ��ѯ�豸״̬    NULL                     */
	UPIL_ASK_DTMF_TX,               /* 14 ���� DTMF ����  1~n λ����, ASCII ֵ     */
	UPIL_ASK_RECORD_UP,             /* 15 ȡ����ȥ���¼  NULL                     */
	UPIL_ASK_GET_VERSION,           /* 16 ȡ�汾��Ϣ      NULL                     */
	UPIL_ASK_AUTHORIZATION,         /* 17 (δ��)                                   */
	UPIL_ASK_READ_MEMORY,           /* 18 ��ȡ�ڴ�        NULL                     */
	UPIL_CTL_RINGER_ON,             /* 19 �����Դ�����-�� NULL                     */
	UPIL_CTL_SET_TIME,              /* 20 ���û�����ʱ��  ������ʱ���� 6 �ֽ�      */
	UPIL_CTL_SPEECH_RECORDING_ON,   /* 21 ͨ��¼��-��ʼ   NULL                     */
	UPIL_CTL_SPEECH_RECORDING_OFF,  /* 22 ͨ��¼��-����   NULL                     */
	UPIL_ASK_BUSY_TONE,             /* 23 ����æ�����    NULL                     */
	UPIL_CTL_SET_FLASH_TIME,        /* 24 ���� F ʱ��     n����*10/78, 1�ֽ�       */
	UPIL_CTL_SET_MEMORY_NUMBER,          /* 25 ���� M ����Ӧ�� ?                        */
	UPIL_ASK_READ_MANUFACTURER_INF, /* 26 ��ȡ���Ҵ���    NULL                     */
	UPIL_RESERVED_CMD,
	UPIL_ASK_READ_SN,               /* 28 ��ȡ���к�      NULL                     */
	UPIL_CTL_WRITE_SN,              /* 29 д�����к�      n �ֽ����к�, �� ASCII   */
	UPIL_ASK_HARDWARE_INF,          /* 30 ��ȡӲ������    NULL                     */
	UPIL_CTL_VOIP_RINGER,           /* 31 (δ��)                                   */
	UPIL_CTL_KEYBOARD_MAP,          /* 32 (δ��)          ?                        */
	UPIL_CTL_DISP_ICON,             /* 33 (δ��)          ?                        */
	UPIL_CTL_PHONE_VOLUME,          /* 34 (δ��)          ?                        */
	UPIL_CTL_PHONE_UNMUTE,          /* 35 �������(����)  NULL                     */
	UPIL_CTL_EEPROM_INIT,           /* 36 (δ��)          ?                        */
	UPIL_CTL_RECORD_LIGHT_ON,       /* 37 ����¼����-��   NULL                     */
	UPIL_CTL_RECORD_LIGHT_OFF,      /* 38 ����¼����-��   NULL                     */
	UPIL_CTL_INI_SET,               /* 39 (δ��)          ?                        */
	UPIL_CTL_BBK_VOIP_ON,           /* 40 (δ��)          NULL                     */
	UPIL_CTL_BBK_VOIP_OFF,          /* 41 (δ��)          NULL                     */

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

//  ������(wParam)           ֵ ˵��          ����
typedef enum UPILEvent
{
    UPIL_EVT_BEGIN=UPIL_CTL_MAX,
	UPIL_EVT_RESERVE=UPIL_EVT_BEGIN,          /* 0  ������Ϣ��    -                               */
	UPIL_EVT_USB_ATTACH,       /* 1  USB�����豸   -                               */
	UPIL_EVT_USB_DETACH,       /* 2  USB�豸�Ƴ�   -                               */
	UPIL_EVT_PC_HOOK_ON,       /* 3  (δ��)                                        */
	UPIL_EVT_PC_HOOK_OFF,      /* 4  (δ��)                                        */
	UPIL_EVT_HAND_HOOK_ON,     /* 5  �ֱ�ժ��      -                               */
	UPIL_EVT_HAND_HOOK_OFF,    /* 6  �ֱ��һ�      -                               */
	UPIL_EVT_HFREE_HOOK_ON,    /* 7  ����ժ��      -                               */
	UPIL_EVT_HFREE_HOOK_OFF,   /* 8  ����һ�      -                               */
	UPIL_EVT_PARALL_HOOK_ON,   /* 9  ����ժ��      -                               */
	UPIL_EVT_PARALL_HOOK_OFF,  /* 10 �����һ�      -                               */
	UPIL_EVT_RING,             /* 11 ��������      -                               */
	UPIL_EVT_KEY_DOWN,         /* 12 �豸����      lParam=enum UsbPhoneKey         */
	UPIL_EVT_PHONE_STATE,      /* 13 �豸״̬��ѯ  lParam=enum UsbPhoneState       */
	UPIL_EVT_CALL_ID,          /* 14 �������      ReadCallId() ��ȡ               */
	UPIL_EVT_DIAL_END,         /* 15 �������      -                               */
	UPIL_EVT_SEC_DTMF_RX,      /* 16 �Է�����      lParam=DTMF ֵ                  */
	UPIL_EVT_RESERVED_1,
	UPIL_EVT_RESERVED_2,
	UPIL_EVT_RESERVED_3,
	UPIL_EVT_RESERVED_4,
	UPIL_EVT_RESERVED_5,
	UPIL_EVT_BUSY_ON,          /* 22 ��⵽æ��    -                               */
	UPIL_EVT_RESERVED_6,
	UPIL_EVT_NEW_CID_TX,       /* 24 δ�������ϴ�  ReadCallId() ��ȡ               */
	UPIL_EVT_OLD_CID_TX,       /* 25 �ѽ������ϴ�  ReadCallId() ��ȡ               */
	UPIL_EVT_DIALED_TX,        /* 26 ���������ϴ�  ReadCallId() ��ȡ               */
	UPIL_EVT_HISTORY_FINISHED, /* 27 ��¼�ϴ����  -                               */
	UPIL_EVT_VERSION,          /* 28 �汾��        lParam=version                  */
	UPIL_EVT_RESTART,          /* 29 Ӳ������/���� -                               */
	UPIL_EVT_REDIAL,           /* 30 �������� DTMF lParam=KEY_0 ~ P                */
	UPIL_EVT_BAT_CRITICAL,     /* 31 ��ص͵�      -                               */
	UPIL_EVT_VOIP_HOOK_OFF,    /* 32 (δ��)                                        */
	UPIL_EVT_VOIP_HOOK_ON,     /* 33 (δ��)                                        */
	UPIL_EVT_PARAL_DTMF,       /* 34 �������� DTMF lParam=DTMF ֵ                  */
	UPIL_MSG_VENDOR,           /* 35 ���Ҵ���      ReadCallId() ��ȡ               */
	UPIL_MSG_SERIAL,           /* 36 ���к�        ReadCallId() ��ȡ               */
	UPIL_MSG_HW_TYPE,          /* 37 Ӳ������      lParam=UsbPhoneType             */
	UPIL_EVT_VOLUM_CHANGE,     /* 38 (δ��)                                        */

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

