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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <media/AudioSystem.h>
#include <media/AudioRecord.h>
#include <media/AudioTrack.h>
#include <system/audio.h>

#include "asoundlib.h"

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "UPIL_AUDIO"
#include <utils/Log.h>

typedef void (*callfunc)();

static callfunc s_play_endfunc;
static char s_play_filename[2048];
static callfunc s_record_endfunc;
static char s_record_filename[2048];
static int s_record_timeout = 0;

static pthread_t s_tid_audio_in = NULL;
static pthread_t s_tid_audio_out = NULL;
static pthread_t s_tid_audio_play = NULL;
static pthread_t s_tid_audio_record = NULL;

static int s_track_ready = 0;
static int s_audio_running = 0;

#if 0
struct audio_thread_param {
    int is_running;
    pthread_t tid;
    char* filename;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    callfunc exit_func;
};

static struct audio_thread_param s_param_audio_in;
static struct audio_thread_param s_param_audio_out;
static struct audio_thread_param s_param_audio_play;
static struct audio_thread_param s_param_audio_record;
#endif

static struct pcm_config pcm_config_ua_out = {
    channels : 2,
    rate : 48000,
    period_size : 1024,
    period_count : 4,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

static struct pcm_config pcm_config_ua_in = {
    channels : 1,
    rate : 8000,
    period_size : 512,
    period_count : 2,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

#define MAX_BUF_SIZE    128*1024

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

namespace {

using namespace android;

static AudioTrack s_host_track;
static int is_host_track_setup = 0;
#define USB_AUDIO_CARD      1
#define USB_AUDIO_DEVICE    0

static int hosta_setup_track(AudioTrack& track, size_t* buf_size)
{
    int frame_count;

    if (AudioTrack::getMinFrameCount(&frame_count, AUDIO_STREAM_MUSIC,
                44100) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return -1;
    }
    LOGD("reported AudioTrack frame count: %d", frame_count);

    if (!is_host_track_setup) {
        if (track.set(AUDIO_STREAM_MUSIC, 44100, AUDIO_FORMAT_PCM_16_BIT,
                AUDIO_CHANNEL_OUT_STEREO, frame_count) != NO_ERROR) {
            LOGE("cannot initialize audio device");
            return -1;
        }
        is_host_track_setup = 1;
    }

    LOGD("latency: output %d", track.latency());

    *buf_size = frame_count;//*track.frameSize();
    
    return 0;
}

static int hosta_setup_record(AudioRecord& record, size_t* buf_size)
{
    int frame_count;
    if (AudioRecord::getMinFrameCount(&frame_count, 44100,
        AUDIO_FORMAT_PCM_16_BIT, 2) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return -1;
    }
    LOGD("Local record's frame count: %d", frame_count);

/*
    if (frame_count < sampleCount * 2) {
        frame_count = sampleCount * 2;
    }
*/
    if (record.set(AUDIO_SOURCE_VOICE_COMMUNICATION, 44100, AUDIO_FORMAT_PCM_16_BIT,
        AUDIO_CHANNEL_IN_STEREO, frame_count) != NO_ERROR) {
        LOGE("cannot initialize host audio record");
        return -1;
    }

    *buf_size = frame_count*record.frameSize();

    return 0;
}

static int usba_setup_record(struct usb_audio_pcm **ua_record, size_t* buf_size)
{
    struct usb_audio_pcm *record = NULL;
    
    record = usb_audio_open(USB_AUDIO_CARD, USB_AUDIO_DEVICE, 
                        PCM_IN, &pcm_config_ua_in);
    if (!record){
        LOGE("cannot open usb audio device");
        return -1;
    }

    if (0 != usb_audio_set_resample(record, 44100, 2, 5))
    {
        LOGE("resample setup failed\n");
        if (record) usb_audio_close(record);
        return -1;
    }

    *buf_size = usb_audio_get_buffer_size(record);
    *ua_record = record;

    return 0;
}

static int usba_setup_track(struct usb_audio_pcm **ua_track, size_t* buf_size,
                            int rate, int channel)
{
    struct usb_audio_pcm* track = NULL;
    track = usb_audio_open(USB_AUDIO_CARD, USB_AUDIO_DEVICE, 
                        PCM_OUT, &pcm_config_ua_out);
    if (!track) {
        LOGE("cannot open usb audio device");
        return -1;
    }

    if (0 != usb_audio_set_resample(track, rate, channel, 5))
    {
        LOGE("resample setup failed\n");
        if (track) usb_audio_close(track);
        return -1;
    }

    if (0 != usb_audio_set_volume(track, 12))
    {
        LOGE("set volume failed\n");
        if (track) usb_audio_close(track);
        return -1;
    }

    *buf_size = usb_audio_get_buffer_size(track);
    *ua_track = track;

    return 0;
}

static int hosta_read_data(AudioRecord* record, char* data, size_t count)
{
    int frame_count = count/record->frameSize();
    int remain_frame_count = frame_count;
    int chances = 100;

    if (frame_count <= 0)
        return 0;
    
    while (--chances > 0 && remain_frame_count > 0) {
        AudioRecord::Buffer buffer;
        buffer.frameCount = remain_frame_count;
        status_t status = record->obtainBuffer(&buffer, 1);
        LOGV("obtainBuffer: %d", status);

        if (status == NO_ERROR) {
            int offset = (frame_count - remain_frame_count)*record->frameSize();
            memcpy(&data[offset], buffer.i8, buffer.size);
            remain_frame_count -= buffer.frameCount;
            record->releaseBuffer(&buffer);
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            LOGE("cannot read frome AudioRecord");
            return -1;
        }
    }

    if (chances<=0) return -1;

    return 0;
}

/*
    把声音数据写到track，从而在系统中播放出来
 */
static int hosta_write_data(AudioTrack* track, char* data, size_t count)
{
    int frame_count = count/track->frameSize();
    int remain_frame_count = frame_count;
    int chances = 100;

    if (frame_count <= 0)
        return 0;

    while (--chances > 0 && remain_frame_count > 0) {
        AudioTrack::Buffer buffer;
        LOGV("remain_frame_count=%d", remain_frame_count);
        buffer.frameCount = remain_frame_count;
        status_t status = track->obtainBuffer(&buffer, 1);
        
        if (status == NO_ERROR) {
            int offset = (frame_count-remain_frame_count)*track->frameSize();

            memcpy(buffer.i8, &data[offset], buffer.size);
            remain_frame_count -= buffer.frameCount;
            track->releaseBuffer(&buffer);
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            LOGE("cannot write to AudioTrack");
            break;
        }
    }
    
    if (chances<=0) {
        LOGE("chances<=0");
        return -1;
    }

    return 0;
}

/*
    从 USB Audio 的record获取数据，并写入 HOST Audio 的track
    从而可以听到对方的说话
    留言功能: 将USB Audio的record数据直接保存成文件
 */
// 从固话得到音频数据后，送往HOST进行播放(声音可能可能输出给喇叭/耳机/蓝牙)
// USB Audio(Record) ==> Host Audio(Track)
static void* usba_to_hosta(void* p)
{
    size_t size, size_in, size_out;
    char buffer[MAX_BUF_SIZE];
    int send_track_count = 0;
    struct usb_audio_pcm *usba_record = NULL;

    if (usba_setup_record(&usba_record, &size_in)<0)
        return 0;

    if (hosta_setup_track(s_host_track, &size_out)<0)
        return 0;

    size = 1024;//(size_in>size_out)?size_out:size_in;
    
    LOGD("pcm in %d, pcm out %d, buffer size: %d", size_in, size_out, size);

    s_host_track.start();

    while (s_audio_running) {
        int r_bytes = usb_audio_read(usba_record, buffer, MAX_BUF_SIZE, size);

        if (r_bytes<0) {
            LOGE("usb_audio_read failed: %d", r_bytes);
            break;
        }

        if (hosta_write_data(&s_host_track, buffer, r_bytes)<0)
        {
            LOGE("hosta_write_data failed");
            break;
        }

        if (s_track_ready==0 && ++send_track_count>=5)
            s_track_ready = 1;
    }

    LOGD("host audio track stop");
    usb_audio_close(usba_record);
    s_host_track.stop();
    s_audio_running = 0;

    LOGD("End usba_to_hosta\n");
    
    return 0;
}

/*
    从 Host Audio 的record获取数据，并写入 USB Audio 的track
    从而让通话的对方可以听到本地的声音
    留言功能: 从Host的音频文件中获取数据并写入USB Audio 的track
 */
// 从HOST获取到音频数据后，送往固话，从而让对方听到听音
// Host Audio(Record) ==> USB Audio(Track)
static void* hosta_to_usba(void* param)
{
    size_t buf_size, size_in, size_out;
    char buffer[MAX_BUF_SIZE];
    struct usb_audio_pcm *usba_track = NULL;
    AudioRecord host_record;

    if (hosta_setup_record(host_record, &size_in)<0)
        return 0;

    if (usba_setup_track(&usba_track, &size_out, 44100, 2)<0)
        return 0;

    buf_size = (size_in>size_out)?size_out:size_in;

    LOGD("pcm in %d, pcm out %d, buffer: %d", size_in, size_out, buf_size);
    
//    prepare_record();
    
    LOGD("host audio record start");
    host_record.start();

    while (s_audio_running) {
        if (hosta_read_data(&host_record, buffer, buf_size)<0) {
            LOGE("Error receive host record data\n");
            break;
        }

        if (usb_audio_write(usba_track, buffer, buf_size)<0) {
            LOGE("Error playing sample\n");
            break;
        }
    }

    LOGD("audio record stop");
    usb_audio_close(usba_track);
    host_record.stop();
    s_audio_running = 0;

    LOGD("End hosta_to_usba\n");
    
    return 0;
}

static int file_setup_record(const char* filename, FILE** record, int* rate, int* channel)
{
    FILE *file;
    struct wav_header header;

    file = fopen(filename, "rb");
    if (!file) {
        LOGE("Unable to open file '%s'\n", filename);
        return -1;
    }

    fread(&header, sizeof(struct wav_header), 1, file);

    if ((header.riff_id != ID_RIFF) ||
        (header.riff_fmt != ID_WAVE) ||
        (header.fmt_id != ID_FMT) ||
        (header.audio_format != FORMAT_PCM) ||
        (header.fmt_sz != 16)) {
        LOGE("Error: '%s' is not a PCM riff/wave file\n", filename);
        fclose(file);
        return -1;
    }

    if (header.bits_per_sample != 16)
    {
        LOGE("Error: support 16bits only\n");
        fclose(file);
        return -1;
    }

    *record = file;
    *rate = header.sample_rate;
    *channel = header.num_channels;
    
    return 0;
}

// 从文件中获取到音频数据后，送往固话，从而让对方听到听音。用于电话留言功能
// File ==> USB Audio(Track)
static void* file_to_usba(void* param)
{
    size_t buf_size;
    char buffer[MAX_BUF_SIZE];
    struct usb_audio_pcm *usba_track = NULL;
    FILE* fp_record = NULL;
    int r_bytes = 0;
    int rate = 44100;
    int channel = 2;

    if (file_setup_record(s_play_filename, &fp_record, &rate, &channel)<0)
        return 0;

    if (usba_setup_track(&usba_track, &buf_size, rate, channel)<0)
        return 0;

    LOGD("buffer: %d", buf_size);

    while (s_audio_running) {
        r_bytes = fread(buffer, 1, buf_size, fp_record);
        if (r_bytes<0) {
            LOGE("Error receive data from file\n");
            break;
        } else if (r_bytes==0)
            break;

        if (usb_audio_write(usba_track, buffer, r_bytes)<0) {
            LOGE("Error playing sample\n");
            break;
        }
    }

    LOGD("audio record stop");
    usb_audio_close(usba_track);
    fclose(fp_record);

// 如果是留言提示播放完成，则调用s_play_endfunc开始录音
    if (s_audio_running)
        s_play_endfunc();

    s_audio_running = 0;
    return NULL;
}

static int file_setup_track(struct wav_header *header, 
                const char* filename, FILE** track, 
                unsigned int rate, unsigned int channels)
{
    FILE *file;
    unsigned int bits = 16;

    file = fopen(filename, "wb");
    if (!file) {
        LOGE("Unable to create file '%s'\n", filename);
        return -1;
    }

    header->riff_id = ID_RIFF;
    header->riff_sz = 0;
    header->riff_fmt = ID_WAVE;
    header->fmt_id = ID_FMT;
    header->fmt_sz = 16;
    header->audio_format = FORMAT_PCM;
    header->num_channels = channels;
    header->sample_rate = rate;
    header->bits_per_sample = bits;
    header->byte_rate = (header->bits_per_sample / 8) * channels * rate;
    header->block_align = channels * (header->bits_per_sample / 8);
    header->data_id = ID_DATA;

    /* leave enough room for header */
    fseek(file, sizeof(struct wav_header), SEEK_SET);

    *track = file;

    return 0;
}

int tim_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
    int nsec;
    if ( x->tv_sec > y->tv_sec )
        return -1;
    if ((x->tv_sec==y->tv_sec) && (x->tv_usec>y->tv_usec))
        return -1;
    result->tv_sec = ( y->tv_sec-x->tv_sec );
    result->tv_usec = ( y->tv_usec-x->tv_usec );
    if (result->tv_usec<0)
    {
        result->tv_sec--;
        result->tv_usec+=1000000;
    }
    return 0;
}

// 从固话得到音频数据后，保存成文件，供以后播放。用于电话留言功能
// USB Audio(Record) ==> File
static void* usba_to_file(void* param)
{
    size_t size;
    char buffer[MAX_BUF_SIZE];
    struct usb_audio_pcm *usba_record = NULL;
    FILE* fp_track;
    struct wav_header header;
    unsigned int bytes_read = 0;
    struct timeval start,stop,diff;
    gettimeofday(&start,0);

    if (usba_setup_record(&usba_record, &size)<0)
        return 0;

    if (file_setup_track(&header, s_record_filename, &fp_track, 44100, 2)<0)
        return 0;

    LOGD("usba_record size=%d", size);
    size = 1024;
    
    while (s_audio_running) {
        int r_bytes = usb_audio_read(usba_record, buffer, MAX_BUF_SIZE, size);

        if (r_bytes<0) {
            LOGE("usb_audio_read failed: %d", r_bytes);
            break;
        }

        if (fwrite(buffer, 1, r_bytes, fp_track)!=r_bytes)
        {
            LOGE("fwrite failed: %d", errno);
            break;
        }
        bytes_read += r_bytes;

// 当录音时间超过给定时间时，退出录音状态
        if (s_record_timeout>0) {
            gettimeofday(&stop,0);
            tim_subtract(&diff,&start,&stop);
            if (diff.tv_sec>=s_record_timeout)
                break;
        }
    }

    usb_audio_close(usba_record);

    header.data_sz = (bytes_read/4) * header.block_align;
    fseek(fp_track, 0, SEEK_SET);
    fwrite(&header, sizeof(struct wav_header), 1, fp_track);
    fclose(fp_track);
    s_audio_running = 0;

// 当录音超时结束时，调用s_record_endfunc回调来通知状态机
    if (diff.tv_sec>=s_record_timeout)
        s_record_endfunc();

    LOGD("End usba_to_file\n");

    return 0;
}

#if 0
int prepare_record()
{
    AudioTrack track;
    int frame_count;
    char* buffer;
    size_t size;
    
    if (AudioTrack::getMinFrameCount(&frame_count, AUDIO_STREAM_MUSIC,
        44100) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return 0;
    }

    LOGD("reported frame count: output %d", frame_count);
    if (track.set(AUDIO_STREAM_MUSIC, 44100, AUDIO_FORMAT_PCM_16_BIT,
            AUDIO_CHANNEL_OUT_STEREO, frame_count) != NO_ERROR) {
        LOGE("cannot initialize audio device");
        return 0;
    }

    size = frame_count*track.frameSize();

    buffer = (char*)malloc(size);
    memset(buffer, 0, size);
    
    LOGD("track.start");
    track.start();

    for (int i=0; i<5; i++) {
        int num_read = size;
        int w_frame_count = num_read/track.frameSize();
        frame_count = w_frame_count;
        int chances = 100;
        
        LOGD("num_read %d, frameSize %d, w_frame_count %d", num_read, 
            track.frameSize(), w_frame_count);
        
        while (--chances > 0 && w_frame_count > 0) {
            AudioTrack::Buffer at_buffer;
            at_buffer.frameCount = w_frame_count;
            status_t status = track.obtainBuffer(&at_buffer, 1);
//            LOGD("status %d", status);
            if (status == NO_ERROR) {
                int offset = frame_count - w_frame_count;
                LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
                    offset, at_buffer.frameCount, at_buffer.size);
                memcpy(at_buffer.i8, &buffer[offset*track.frameSize()], at_buffer.size);
                w_frame_count -= at_buffer.frameCount;
                track.releaseBuffer(&at_buffer);
            } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
                LOGE("cannot write to AudioTrack");
                break;
            }
        }
    }

    free(buffer);
    track.stop();
    
    return 0;
}
#endif

}

int start_sound_in_thread()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_audio_in, &attr, usba_to_hosta, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

int start_sound_out_thread()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_audio_out, &attr, hosta_to_usba, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

int start_play_thread()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_audio_play, &attr, file_to_usba, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

int start_record_thread()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_audio_record, &attr, usba_to_file, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

#if 0
static void sigint_handler(int sig)
{
}

int main(int argc, char **argv)
{
    /* install signal handler and begin capturing */
   //signal(SIGINT, sigint_handler);
   //signal(SIGTERM, sigint_handler);

    s_track_ready = 0;
    LOGD("start_sound_in_thread");
    start_sound_in_thread();
    
    while (!g_track_ready)
        usleep(5000);

    LOGD("start_sound_out_thread");
    start_sound_out_thread();

    while(1) ;

    return 0;
}
#endif

extern "C"{

// 结束通话
int upil_audio_stop()
{
    int try_count = 30;
    LOGD("Enter upil_audio_stop\n");

    s_audio_running = 0;

    while (try_count-->0)
    {
        if ( (!s_tid_audio_in || pthread_kill(s_tid_audio_in,0))
            && (!s_tid_audio_out || pthread_kill(s_tid_audio_out,0))
            && (!s_tid_audio_play || pthread_kill(s_tid_audio_play,0))
            && (!s_tid_audio_record || pthread_kill(s_tid_audio_record,0))
            ) {
            LOGI("All audio thread is exited!\n");
            break;
        }
            
        usleep(100*1000);
    }

    if (try_count<=0)
        LOGD("Stop audio timeout");

    s_tid_audio_in = NULL;
    s_tid_audio_out = NULL;
    s_tid_audio_play = NULL;
    s_tid_audio_record = NULL;

    LOGD("End upil_audio_stop");
    return 0;
}

// 开始通话
int upil_audio_start()
{
    if (!s_audio_running)
    {
        s_audio_running = 1;
        s_track_ready = 0;
        
        LOGD("start_sound_in_thread");
        if (start_sound_in_thread()<0)
        {
            s_audio_running = 0;
            return -1;
        }

        {
            int try_count = 20;
            LOGD("wait for audio track ready");
            while (try_count-->0) {
                usleep(100*1000);
                if (s_track_ready)
                    break;
            }
            if (try_count<=0)
                LOGW("track not ready!");
        }

        LOGD("start_sound_out_thread");
        if (start_sound_out_thread()<0)
        {
            upil_audio_stop();
            return -1;
        }
    } else {
        LOGD("Audio has been start!");
    }

    return 0;
}


#if 0
int upil_audio_sound_in_start()
{
    struct audio_thread_param* param = &s_param_audio_in;
    
    param->is_running = 1;
    param->filename = NULL;
    pthread_mutex_init(&param->mutex, NULL);
    pthread_cond_init(&param->cond,NULL);
    param->exit_func = NULL;

    if (pthread_create(&param->tid, NULL, usba_to_hosta, (void*)param) < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }
    
    return 0;
}

static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *) NULL);

    /* what's really funny about this is that I know
       pthread_cond_timedwait just turns around and makes this
       a relative time again */
    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
}

int upil_audio_sound_in_stop()
{
    struct audio_thread_param* param = &s_param_audio_in;
    int ret = 0;

    if (param->is_running) {
        struct timespec ts;
        int err=0;
        setTimespecRelative(&ts, 2000);
        
        err = pthread_cond_timedwait(&param->cond, &param->mutex, &ts);
        if (err == ETIMEDOUT)
            ret = -1;

        pthread_mutex_destroy(param->mutex);
        pthread_cond_destroy(param->cond);
        param->is_running = 0;
    }

    return ret;
}
#endif

/*
    在新的线程中，从指定文件获取音频数据并发送给usb track
 */
int upil_audio_play_file(const char* filename, callfunc func)
{
    if (!s_audio_running)
    {
        s_audio_running = 1;
        s_play_endfunc = func;
        sprintf(s_play_filename, "%s", filename);

        if (start_play_thread()<0)
        {
            s_audio_running = 0;
            return -1;
        }
    } else {
        LOGD("Audio has been start!");
    }
    return 0;
}

int upil_audio_record_file(const char* filename, callfunc func, int timeout)
{
    if (!s_audio_running)
    {
        s_audio_running = 1;
        s_record_endfunc = func;
        s_record_timeout = timeout;
        sprintf(s_record_filename, "%s", filename);

        if (start_record_thread()<0)
        {
            s_audio_running = 0;
            return -1;
        }
    } else {
        LOGD("Audio has been start!");
    }
    
    return 0;
}

}

