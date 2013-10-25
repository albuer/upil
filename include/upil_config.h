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

#ifndef __UPIL_CONFIG_H__
#define __UPIL_CONFIG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <upil_types.h>

#include <upil_sm.h>
#include <tp_protocol.h>
#include <upil_request.h>
#include <upil_error.h>

#define UPIL_SOCKET_CMD    "upil.cmd"
#define UPIL_SOCKET_EVT    "upil.evt"
#define UPIL_SOCKET_RIL    "upil.ril"

#define UPIL_SOCKET_CMD_PATH    "/dev/socket/"UPIL_SOCKET_CMD
#define UPIL_SOCKET_EVT_PATH    "/dev/socket/"UPIL_SOCKET_EVT
#define UPIL_SOCKET_RIL_PATH    "/dev/socket/"UPIL_SOCKET_RIL

#define UPIL_MAX_MSG_LEN    512
#define UPIL_MAX_PATH       2048

//#define EVENT_LOOP_DEBUG
#define ANDROID_LOG
#define DEBUG 1

#include <upil_debug.h>

#endif

