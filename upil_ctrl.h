#ifndef __UPIL_CTRL_H__
#define __UPIL_CTRL_H__

#if __cplusplus
extern "C" {
#endif

/*
 * 应用程序通过该函数连接到upil库中的cmd接口
 * 以后应用程序就能通过该接口向upil库发送请求
 */
int upil_ctrl_connect(void);

int upil_ctrl_disconnect(void);

int upil_ctrl_dial(const char* number);

int upil_ctrl_hangup();

int upil_ctrl_get_version(char* version, int version_size);

int upil_ctrl_request(const char *cmd, char *reply, size_t reply_len);

int upil_monitor_connect();

int upil_monitor_recv(char *buf, size_t buf_size);

int upil_monitor_disconnect();

int upil_connect();

void upil_disconnect();

int upil_is_device_exist();

int upil_is_device_ready();

int upil_ril_write(int fd, const char* buffer, size_t buf_size);

int upil_ril_read(int fd, char* buffer, size_t buf_size);

#if __cplusplus
};  // extern "C"
#endif

#endif
