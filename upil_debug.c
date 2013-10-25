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
#include <string.h>
#include <upil_config.h>

#define LOG_TAG "UPIL"
#include <utils/Log.h>

#define MAX_MSG_SIZE        1024

int g_dbg_count = 0;

int upil_debug_level=MSG_INFO;
int upil_debug_ts=0;

void upil_printf(int level, char *format, ...)
{
	if (level >= upil_debug_level) {
		va_list ap;
		if (level == MSG_ERROR)
			level = ANDROID_LOG_ERROR;
		else if (level == MSG_WARNING)
			level = ANDROID_LOG_WARN;
		else if (level == MSG_INFO)
			level = ANDROID_LOG_INFO;
		else
			level = ANDROID_LOG_DEBUG;

        va_start(ap, format);
        if (upil_debug_ts)
        {
            char buf[MAX_MSG_SIZE];
            struct timeval tv;
            gettimeofday(&tv, NULL);

            vsnprintf(buf, MAX_MSG_SIZE, format, ap);
            va_end(ap);
            __android_log_print(level, LOG_TAG, "{%ld.%06u} %s",
                                (long)tv.tv_sec, (uint)tv.tv_usec, buf);
        } else {
            __android_log_vprint(level, LOG_TAG, format, ap);
            va_end(ap);
        }
	}
}

void upil_hexdump(int level, const char *title, const uchar *data, size_t len)
{
    size_t i=0;
    char line[MAX_MSG_SIZE] = "";
    char sbyte[4];

    sprintf(line, "%s ", title);

    if (data==NULL || len<=0)
    {
        strcat(line, " [NULL]\n");

        upil_printf(level, line);
        
        return;
    }
    
    for(;i<len;i++)
    {
        sprintf(sbyte, "%02X ", data[i]);
        strcat(line, sbyte);
    }
    strcat(line, "\n");

    upil_printf(level, line);
}

