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
#include <unistd.h>

#include <upil_config.h>
#include <upil_sm.h>
#include <upil.h>

// Telephone Protocol Analyzer
#define TP_FLAG                 0xFF
#define HID_FLAG                0x03

#define HIDPDU_LINE_BYTES       8
#define TPPDU_LINE_BYTES        6
#define HIDPUD_LINE_MAX         10
#define MAX_EVENT_DATA          512

typedef struct TPPDU_t{
    uchar flag;
    uchar cmd_size;
    uchar cmd;
    uchar param[1];
}__attribute__((packed)) TPPDU_t;

typedef struct HIDPDU_t{
    uchar flag;
    uchar tp_size;
    uchar tp_pdu[1];
}__attribute__((packed)) HIDPDU_t;

typedef struct TPPReq_t {
    uchar id;
    uchar data_size;
    uchar data[MAX_EVENT_DATA];
}__attribute__((packed)) TPPReq_t,TPPEvt_t;

#define TPP_CMD_ONEBYTE(data,id) \
    do{\
        memcpy((uchar*)data, "\x03\x03\xFF\x01\xCC\x05\x06\x07", HIDPDU_LINE_BYTES); \
        *((uchar*)data+4) = (uchar)id; \
    }while(0)

#define TPP_HIDPDU_INIT(data)  memcpy((uchar*)data, "\x03\x01\x02\x03\x04\x05\x06\x07", HIDPDU_LINE_BYTES);

// 用该信号来同步HID指令的发送
static pthread_cond_t s_cmd_completed_cond = PTHREAD_COND_INITIALIZER;

static uchar s_pdu_buf[HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX] = "";
static int s_pdu_offset=0;

// 发送指令
// ==> 03 03 ff 01 6a 05 06 07
// 接收返回
// <== 03 80 ff 03 11 64 08 07
// <== 03 02 ff 01 08 64 08 07
// <== 03 03 02 ec 08 64 08 07
// <== 03 01 d3 01 08 64 08 07

/*
 * 检测新TP PDU的完整性
 * 参数:
 *     tp_pdu       目前已经获取到的TP PDU数据
 *     tp_size      TP PDU数据大小
 * 返回值:
 *     1    完整的TP PDU数据包
 *     0    不完整，且未发现异常
 *     -1   有异常的TP PDU
 */
static int check_tppdu(TPPDU_t* tp_pdu, int tp_size)
{
    int remain_size = 0;
    int ret;
    
    if (tp_size<=0)
        return 0;
    
    if (tp_pdu->flag != TP_FLAG)
    {
        return -1;
    }

    if (tp_size<2)
        return 0;
    
    remain_size = tp_pdu->cmd_size+2-tp_size;

    return remain_size?(remain_size>0?0:-1):1;
}

typedef struct FSK_CID_t{
    uchar mode;
    uchar len;
    uchar month;
    uchar day;
    uchar hour;
    uchar minute;
    uchar cid[1];
}__attribute__((packed)) FSK_CID;

static void fsk_to_string(char* msg, void* data)
{
    FSK_CID* fsk = data;
    uchar* cid = fsk->cid;
    char* cid_str;
#if 0
    sprintf(msg, "%02d-%02d %02d:%02d ", fsk->month, fsk->day, 
                        fsk->hour, fsk->minute);

    cid_str = msg+strlen(msg);
#endif

    cid_str = msg;

    while(1) {
        uchar n = ((*cid>>4) & 0x0F);
        if (n==0x0F||n>9) break;
        *cid_str = '0'+n;
        ++cid_str;
        n = *cid & 0x0F;
        if (n==0x0F||n>9) break;
        *cid_str = '0'+n;
        ++cid_str;
        ++cid;
    };
    *cid_str = '\0';
}

static int tppdu2event(TPPDU_t* tp_pdu, TPPEvt_t* event)
{
    event->id = UPIL_EVT_UNKOWN;
    event->data_size = 0;
    
    switch (tp_pdu->cmd)
    {
    case TPP_EVT_HOOK_ON:
    case TPP_EVT_HKS_ON:
    case TPP_EVT_PARALLEL_ON:
    case TPP_EVT_VOIP_PHONE_HOOK_ON:
        event->id = UPIL_EVT_HFREE_HOOK_ON;
        break;
        
    case TPP_EVT_HOOK_OFF:
    case TPP_EVT_HKS_OFF:
    case TPP_EVT_PARALLEL_OFF:
    case TPP_EVT_VOIP_PHONE_HOOK_OFF:
        event->id = UPIL_EVT_HFREE_HOOK_OFF;
        break;
        
    case TPP_EVT_BUSY_ON:
        event->id = UPIL_EVT_BUSY_ON;
        break;

    case TPP_EVT_HISTORY_FINISHED:
        event->id = UPIL_EVT_HISTORY_FINISHED;
        break;
        
    case TPP_MSG_DTMF_TX_FINISHED:
        event->id = UPIL_EVT_DIAL_END;
        break;
        
    case TPP_MSG_PHONE_STATE:
        event->id = UPIL_EVT_PHONE_STATE;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;

    case TPP_EVT_REDIAL_ON:
        event->id = UPIL_EVT_REDIAL;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;

    case TPP_EVT_KEY_DOWN:
        event->id = UPIL_EVT_KEY_DOWN;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;
        
    case TPP_EVT_PARALLEL_DTMF_RX:
        event->id = UPIL_EVT_PARAL_DTMF;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;
        
    case TPP_EVT_ICM_DTMF_RX:
    case TPP_EVT_DTMF_RX:
        event->id = UPIL_EVT_SEC_DTMF_RX;
        event->data_size = tp_pdu->cmd_size-1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;

    case TPP_EVT_DIALED_TX:
        event->id = UPIL_EVT_DIALED_TX;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;

    case TPP_MSG_HARDWARE_INF:
        event->id = UPIL_MSG_HW_TYPE;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;
        
    case TPP_MSG_SN:
        event->id = UPIL_MSG_SERIAL;
        event->data_size = 8;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;
        
    case TPP_MSG_MANUFACTURER_INF:
        event->id = UPIL_MSG_VENDOR;
        event->data_size = 8;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;
        
    case TPP_MSG_VERSION:
        event->id = UPIL_MSG_VENDOR;
        event->data_size = 1;
        memcpy(event->data, tp_pdu->param, event->data_size);
        break;

    case TPP_EVT_RING:
        event->id = UPIL_EVT_RING;
        break;

    case TPP_EVT_FSK_RX:
        event->id = UPIL_EVT_CALL_ID;
        fsk_to_string((char*)event->data, (void*)tp_pdu->param);
        event->data_size = strlen((char*)event->data);
        upil_dbg("Get FSK CID: %s\n", (char*)event->data);
        break;

    default:
        break;
    }

    return 0;
}

static int make_cmd(uchar* reply, uchar id, uchar* param, uchar param_s)
{
    uchar pdu[HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX];
    uchar* data = pdu;
    HIDPDU_t* hidpdu = (HIDPDU_t*)reply;
    TPPDU_t* tppdu = (TPPDU_t*)data;
    int tppdu_size = 0;
    int line = 0;

    ENTER_LOG();
    
    tppdu->flag = TP_FLAG;
    tppdu->cmd_size = 1+param_s;
    tppdu->cmd = id;

    memcpy(tppdu->param, param, param_s);
    
    tppdu_size = tppdu->cmd_size+2;

    upil_hexdump(MSG_MSGDUMP, "TP PDU:", data, tppdu_size);

    while (tppdu_size>0) {
        int len;
        TPP_HIDPDU_INIT(hidpdu);
        len = tppdu_size>TPPDU_LINE_BYTES?TPPDU_LINE_BYTES:tppdu_size;
        hidpdu->tp_size = len;
        memcpy(hidpdu->tp_pdu, data, len);
        tppdu_size -= len;
        data += len;

        upil_hexdump(MSG_MSGDUMP, "HID PDU:", (uchar*)hidpdu, HIDPDU_LINE_BYTES);
        reply += HIDPDU_LINE_BYTES;
        hidpdu = (HIDPDU_t*)reply;

        ++line;
    }

    upil_printf(MSG_MSGDUMP, "hid pdu line: %d\n", line);

    LEAVE_LOG();
    return line;
}

static uchar make_dial_param(uchar* reply, uchar speed, const char* number)
{
    int i=0;
    uchar result = 0;

    upil_info("Len=%d: %s", strlen(number), number);
    reply[result++] = speed;
    while (number[i])
    {
        upil_info("number: 0x%X", number[i]);
        if (number[i] == '*')
            reply[result++] = 0xA;
        else if (number[i] == '#')
            reply[result++] = 0xB;
        else if (number[i] == 'P')
            reply[result++] = 0xC;
        else if (number[i] == 'B')
            reply[result++] = 0xD;
        else if (number[i] == 'C')
            reply[result++] = 0xE;
        else if (number[i]>='0' && number[i] <= '9')
            reply[result++] = number[i]-'0';
        else
            upil_warn("unknown number");

        ++i;
    }

    upil_info("result=%d", result);

    return result;
}

/*
 * 解析得到新的数据单元后，再进行解析
 * 参数:
 *     data         新接收到的HID原始数据
 *     reply        先前已经得到但不完整的TP数据
 *     event        TP数据完整后，解析得到的事件通过该参数返回
 * 返回值:
 *     解析后的 TP PDU数据出现异常，则返回-1
 *     解析后的 TP PDU数据还未完整，则返回0
 *     解析后的 TP PDU数据完整，得到event/param等，则返回1
 */
int hidpdu2event(uchar* data, uchar* reply, int* tp_index, TPPEvt_t* event)
{
    HIDPDU_t* hid_pdu = (HIDPDU_t*)data;
    TPPDU_t* tp_pdu = (TPPDU_t*)reply;
    int ret=0;
    
    ENTER_LOG();
    if (hid_pdu->flag == HID_FLAG) {
        memcpy((uchar*)tp_pdu+*tp_index, hid_pdu->tp_pdu, hid_pdu->tp_size);
        *tp_index += hid_pdu->tp_size;
    } else {
        upil_error("*** Bad HID PDU!\n");
        LEAVE_LOG();
        return -1;
    }

    if (hid_pdu->tp_size>0)
    {
    // 有新数据加入到 TP PDU，可进行检测
        int result = check_tppdu(tp_pdu, *tp_index);
        upil_printf(MSG_MSGDUMP, "check tp pdu result: %d", result);
        if (result>0)
        {
            upil_dump("TP PDU Data:", (uchar*)tp_pdu, tp_pdu->cmd_size+2);
            if (event){
                tppdu2event(tp_pdu, event);
                upil_dump("TP PDU to Event:", (uchar*)event, event->data_size+2);
            }
            ret = 1;
        }
        else if (result<0)
            ret = -1;
    }

    LEAVE_LOG();
    return ret;
}

/*
 *  返回指令的行数
 *     由于每次发送命令只能是9个bytes，对于超过9个bytes的命令，需要分割
 *     成多行指令，每行都是9个bytes
 *     如果无法转换成指令，则返回0
 */
static int request2pdu(uchar* reply, TPPReq_t* request)
{
    int line_num = 1;
    switch (request->id)
    {
    case UPIL_CTL_USB_ON:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_USB_RUN);
        break;
        
    case UPIL_CTL_HANDFREE_ON:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_PCHOOK_ON);
        break;
        
    case UPIL_CTL_HANDFREE_OFF:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_PCHOOK_OFF);
        break;
        
    case UPIL_ASK_HARDWARE_INF:
        TPP_CMD_ONEBYTE(reply, TPP_ASK_HARDWARE_INF);
        break;
        
    case UPIL_ASK_PHONE_STATE:
        TPP_CMD_ONEBYTE(reply, TPP_ASK_PHONE_STATE);
        break;
                
    case UPIL_ASK_READ_SN:
        TPP_CMD_ONEBYTE(reply, TPP_ASK_READ_SN);
        break;

    case UPIL_ASK_GET_VERSION:
        TPP_CMD_ONEBYTE(reply, TPP_ASK_GET_VERSION);
        break;

    case UPIL_ASK_RECORD_UP:
        TPP_CMD_ONEBYTE(reply, TPP_ASK_RECORD);
        break;
        
    case UPIL_CTL_RINGER_OFF:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_RINGER_OFF);
        break;

    case UPIL_CTL_RECORD_LINE_PLAY_ON:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_RECORD_LINE_PLAY_ON);
        break;

    case UPIL_CTL_RECORD_LINE_PLAY_OFF:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_RECORD_LINE_PLAY_OFF);
        break;

    case UPIL_ASK_BUSY_TONE:
        TPP_CMD_ONEBYTE(reply, TPP_CTL_BUSY_TONE);
        break;

    // ff 07 22 0c 05 1a 10 39 29 (2012-5-26 16:57:41)
    case UPIL_CTL_SET_TIME:
        if (request->data_size==0)
        {
            uchar data[6];
            time_t timep;
            struct tm *p;
            
            time(&timep);
            p=localtime(&timep);

            upil_info("current time: %04d-%02d-%02d %02d:%02d:%02d",
                (1900+p->tm_year),(1+p->tm_mon), p->tm_mday,
                p->tm_hour, p->tm_min, p->tm_sec);
            
            data[0] = (1900+p->tm_year)-2000;
            data[1] = 1+p->tm_mon;
            data[2] = p->tm_mday;
            data[3] = p->tm_hour;
            data[4] = p->tm_min;
            data[5] = p->tm_sec;
            line_num = make_cmd(reply, TPP_CTL_SET_TIME, data, 6);
        } else {
            line_num = make_cmd(reply, TPP_CTL_SET_TIME, request->data, request->data_size);
        }
        break;
        
    case UPIL_ASK_DTMF_TX:
        {
            uchar param[HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX];
            uchar param_size = make_dial_param(param, 0x64, (const char*)request->data);
            if (param_size>0)
                line_num = make_cmd(reply, TPP_ASK_DTMF_TX, param, param_size);
            else
                line_num = 0;
        }
        break;
        
    default:
        line_num = 0;
        break;
    }
    
    return line_num;
}

/*
 * 当设备有数据可读时，该函数被回调，用来读取设备数据并解析
 * 参数:
 *     fd           - 设备的fd
 *     flags        - 未用
 *     param        - 未用
 * 返回值:
 *     正常返回 0
 *     出现异常，则返回 -1 退出状态机
 */
int tpp_recv_event_cb(int fd, short flags, void *param)
{
    int ret = 0;
    int r_bytes = 0;
    uchar buf[UPIL_MAX_MSG_LEN];
    uchar* pbuf = buf;
    TPPEvt_t event;

    ENTER_LOG();

    r_bytes = read(fd, buf, UPIL_MAX_MSG_LEN);
    upil_printf(MSG_MSGDUMP, "read bytes %d", r_bytes);

    if (r_bytes<0)
    {
        upil_error("read device failure, must exit.\n");
        return -1;
    }
    
    if (r_bytes>0) upil_dump("TP <<", buf, r_bytes);
    
    while (r_bytes >= HIDPDU_LINE_BYTES)
    {
        if (pbuf[0]==0x03 && (pbuf[1]&0x80)==0x80)
        {
            upil_printf(MSG_MSGDUMP, "broadcast cmd complete\n");
            pthread_cond_broadcast(&s_cmd_completed_cond);
            buf[1] &= 0x3F;
#if 1// 这个地方应该还有问题，应继续调试
            // 当收到一个确认包时，将认为上一条上报已经结束
            memset(s_pdu_buf, 0, s_pdu_offset);
            s_pdu_offset = 0;
#endif
        }
        
        ret = hidpdu2event(pbuf, s_pdu_buf, &s_pdu_offset, &event);

        if (ret) {
            if (ret>0)
            {
                upil_dbg("send msg: %d, %p, %d\n", event.id, event.data, event.data_size);
                upil_sm_send_msg(g_upil_s.fsm, event.id, event.data, event.data_size);
            }
            
            memset(s_pdu_buf, 0, s_pdu_offset);
            s_pdu_offset = 0;
        }

        pbuf += HIDPDU_LINE_BYTES;
        r_bytes -= HIDPDU_LINE_BYTES;
    }
    
    LEAVE_LOG();

    return 0;
}


/*
 * 发送请求给设备
 *     一个请求可能需要分隔成多条HID指令来发送，每次发送完一条
 *     HID指令后，需要等待DEV返回确认包，然后才能接着发送下一条HID指令
 * 参数:
 *     req          - 请求号
 *     data         - 参数，可能为空
 *     data_size    - 参数大小
 * 返回值:
 *     0    命令发送完成
 *     -1   命令发送失败
 */
int tpp_send_cmd(UPILReq req, void* data, int data_size)
{
    uchar reply[HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX];
    int line;
    int i=0;
    TPPReq_t tp_req;

    ENTER_LOG();
    
    memset(reply, 0, HIDPDU_LINE_BYTES*HIDPUD_LINE_MAX);

    memset((void*)&tp_req, 0, sizeof(TPPReq_t));
    tp_req.id = req;
    tp_req.data_size = data_size;
    
    if (data_size>12)
        upil_info("=== %d: %s", data_size, (char*)data);
    memcpy(tp_req.data, data, data_size);

    line = request2pdu(reply, &tp_req);

    if (line>0) upil_hexdump(MSG_MSGDUMP, "make HID PDU:", reply, line*HIDPDU_LINE_BYTES);

    for (;i<line;i++)
    {
        upil_dump("TP >>", reply+i*HIDPDU_LINE_BYTES, HIDPDU_LINE_BYTES);
        
        if (write(g_upil_s.device_fd, reply+i*HIDPDU_LINE_BYTES, HIDPDU_LINE_BYTES)
            != HIDPDU_LINE_BYTES)
        {
            upil_error("write error: %d\n", errno);
            LEAVE_LOG();
            return -1;
        }

        upil_printf(MSG_MSGDUMP, "Wait command finish...");
        pthread_cond_wait(&s_cmd_completed_cond, NULL);
    }

    LEAVE_LOG();
    return 0;
}

