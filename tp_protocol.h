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

#ifndef __TP_PROTOCOL_H__
#define __TP_PROTOCOL_H__

// Telephone Protocol

#include <upil_request.h>

typedef enum TPEvent
{
    TPP_EVT_VOLUM_CHANGE=0x5C,     /* 38 (未用)                                        */

    TPP_MSG_VERSION=0x81,
    TPP_MSG_AUTHORIZATION,
    TPP_MSG_MANUFACTURER_INF,
    TPP_MSG_SN,
    TPP_MSG_HARDWARE_INF,
    TPP_MSG_EEPROM_INF,
    
    TPP_MSG_PHONE_STATE=0xAB,
    
    TPP_EVT_FSK_RX=0xD1,
    TPP_EVT_DTMF_RX,            /* 对方按键   lParam=DTMF 值
	                                          KEY_0 ~ 9
	                                          DTMF_HASH
	                                          DTMF_START
	                                          DTMF_A
	                                          DTMF_B
	                                          DTMF_C
	                                          DTMF_D
	                                          DTMF_P                          */
    TPP_MSG_DTMF_TX_FINISHED,
    TPP_EVT_NEW_CID_TX,       /* 24 未接来电上传  ReadCallId() 读取               */
    TPP_EVT_OLD_CID_TX,       /* 25 已接来电上传  ReadCallId() 读取               */
    TPP_EVT_DIALED_TX,        /* 26 拨出号码上传  ReadCallId() 读取               */
    TPP_EVT_HISTORY_FINISHED, /* 27 记录上传完成  -                               */
    TPP_EVT_ICM_DTMF_RX,    /* 录音状态下检测到对方按键 */
    TPP_EVT_PARALLEL_DTMF_RX,

	TPP_EVT_KEY_DOWN=0xE1,         /* 12 设备按键                                      */
    TPP_EVT_CODEC_READY,          /* 0  保留消息字    -                               */
    TPP_EVT_HOOK_ON,    /* 7  免提摘机      -                               */
    TPP_EVT_HOOK_OFF,   /* 8  免提挂机      -                               */
    TPP_EVT_RING,             /* 11 来电振铃      -                               */
    TPP_EVT_RESTART,          /* 29 硬件启动/重启 -                               */
    TPP_EVT_HKS_ON,     /* 5  手柄摘机      -                               */
    TPP_EVT_HKS_OFF,    /* 6  手柄挂机      -                               */
    TPP_EVT_PARALLEL_ON,   /* 9  并机摘机      -                               */
    TPP_EVT_PARALLEL_OFF,  /* 10 并机挂机      -                               */
    TPP_EVT_BUSY_ON,          /* 22 检测到忙音    -                               */
    TPP_EVT_REDIAL_ON,           /* 30 本机拨出 DTMF lParam=KEY_0 ~ P                */
    TPP_EVT_BATTERY_LOW,     /* 31 电池低电      -                               */
    TPP_EVT_VOIP_PHONE_HOOK_ON,
    TPP_EVT_VOIP_PHONE_HOOK_OFF,
} TPEvent;

typedef enum TPCmd
{
    TPP_ASK_GET_VERSION=0x01,

    TPP_ASK_DTMF_TX=0x11,
    TPP_ASK_RECORD,

    TPP_ASK_READ_MANUFACTURER_INF=0x14,
    TPP_CTL_WRITE_MANUFACTURER_INF,
    TPP_ASK_READ_SN,
    TPP_ASK_WRITE_SN,
    TPP_ASK_HARDWARE_INF,
    
    TPP_ASK_PHONE_STATE=0x21,
    TPP_CTL_SET_TIME,
    TPP_CTL_BUSY_TONE,
    
    TPP_CTL_MUTE=0x2A,
    TPP_CTL_LIGHT_ON_LED_OF_OGM=0x2D,
    TPP_CTL_LIGHT_OFF_LED_OF_OGM,
    
    TPP_CTL_PCHOOK_ON=0x61,
    TPP_CTL_PCHOOK_OFF,
    TPP_CTL_RECORD_ON,
    TPP_CTL_RECORD_OFF,
    TPP_CTL_R_FUNC,
    TPP_CTL_RECORD_PLAY_ON,
    TPP_CTL_RECORD_PLAY_OFF,
    TPP_CTL_RECORD_LINE_PLAY_ON,
    TPP_CTL_RECORD_LINE_PLAY_OFF,
    TPP_CTL_USB_RUN,
    TPP_CTL_RINGER_ON,
    TPP_CTL_RINGER_OFF,
} TPCmd;

int tpp_recv_event_cb(int fd, short flags, void *param);
int tpp_send_cmd(UPILReq req, void* data, int data_size);


#endif

