#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#define LOG_NDEBUG 0
#define LOG_TAG "UPIL_APLAY"
#include <utils/Log.h>
#include <media/AudioSystem.h>
#include <media/AudioRecord.h>
#include <media/AudioTrack.h>
#include <system/audio.h>

#include "asoundlib.h"

enum {
    ON_HOLD = 0,
    MUTED = 1,
    NORMAL = 2,
    ECHO_SUPPRESSION = 3,
    LAST_MODE = 3,
};

struct pcm_config pcm_config_ua_out = {
    channels : 2,
    rate : 48000,
    period_size : 1024,
    period_count : 4,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

struct pcm_config pcm_config_ua_in = {
    channels : 1,
    rate : 8000,
    period_size : 512,
    period_count : 2,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

struct pcm_config pcm_config_rt5631_out = {
    channels : 2,
    rate : 44100,
    period_size : 1024,
    period_count : 4,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

struct pcm_config pcm_config_rt5631_in = {
    channels : 2,
    rate : 44100,
    period_size : 2048,
    period_count : 2,
    format : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold : 0,
    silence_threshold : 0,
};

int capturing = 1;

void sigint_handler(int sig)
{
    capturing = 0;
}

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

#if 0
void play_sample(FILE *file, unsigned int card, unsigned int device, 
                 unsigned int channels, unsigned int rate, unsigned int bits)
{
    int num_read;
    AudioTrack track;
    char* buffer;
    int size;
    int frame_count;

    if (AudioTrack::getMinFrameCount(&frame_count, AUDIO_STREAM_MUSIC,
        44100) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return;
    }
    LOGD("reported frame count: output %d", frame_count);

    if (track.set(AUDIO_STREAM_MUSIC, 44100, AUDIO_FORMAT_PCM_16_BIT,
            AUDIO_CHANNEL_OUT_STEREO, frame_count) != NO_ERROR) {
        LOGE("cannot initialize audio device");
        return;
    }

    LOGD("latency: output %d", track.latency());

    size = frame_count*track.frameSize();
    
    buffer = (char*)malloc(size);

    LOGD("track.start");
    track.start();

    do {
        num_read = fread(buffer, 1, size, file);

        int w_frame_count = num_read/track.frameSize();//frame_count;
        frame_count = w_frame_count;
        int chances = 100;

        if (w_frame_count <= 0)
            break;
        
        while (--chances > 0 && w_frame_count > 0) {
            AudioTrack::Buffer at_buffer;
            at_buffer.frameCount = w_frame_count;
            status_t status = track.obtainBuffer(&at_buffer, 1);
            if (status == NO_ERROR) {
                int offset = frame_count - w_frame_count;
//                LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
//                    offset, at_buffer.frameCount, at_buffer.size);
                memcpy(at_buffer.i8, &buffer[offset*track.frameSize()], at_buffer.size);
                w_frame_count -= at_buffer.frameCount;
                track.releaseBuffer(&at_buffer);
            } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
                LOGE("cannot write to AudioTrack");
                break;
            }
        }
    } while (num_read > 0);

    free(buffer);

    track.stop();
}
#endif

int send_track_data(AudioTrack* track, char* data, size_t count)
{
    int frame_count = count/track->frameSize();
    int remain_frame_count = frame_count;
    int chances = 100;

    if (frame_count <= 0)
        return 0;

    while (--chances > 0 && remain_frame_count > 0) {
        AudioTrack::Buffer buffer;
//        LOGD("remain_frame_count=%d", remain_frame_count);
        buffer.frameCount = remain_frame_count;
        status_t status = track->obtainBuffer(&buffer, 1);
        
        if (status == NO_ERROR) {
            int offset = (frame_count-remain_frame_count)*track->frameSize();
            
//            LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
//                offset, buffer.frameCount, buffer.size);
            memcpy(buffer.i8, &data[offset], buffer.size);
            remain_frame_count -= buffer.frameCount;
            track->releaseBuffer(&buffer);
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            LOGE("cannot write to AudioTrack");
            break;
        }
    }
    
    if (chances<=0) return -1;

    return 0;
}

int sound_in()
{
    int ret = 0;
    size_t size, size_in, size_out;
    char buffer_read[MAX_BUF_SIZE];
    int host_card = 0;
    int dev_card = 1;
    int device = 0;
    int host_out_frame_count = 0;
    AudioTrack host_track;
    
    struct usb_audio_pcm *dev_in_pcm = NULL;

    dev_in_pcm = usb_audio_open(dev_card, device, PCM_IN, &pcm_config_ua_in);
    if (!dev_in_pcm) goto start_err;

    ret = usb_audio_set_resample(dev_in_pcm, 
                    pcm_config_rt5631_out.rate,
                    pcm_config_rt5631_out.channels, 5);
    if (ret)
    {
        LOGE("resample setup failed\n");
        goto start_err;
    }

    size_in = usb_audio_get_buffer_size(dev_in_pcm);
    
    if (AudioTrack::getMinFrameCount(&host_out_frame_count, AUDIO_STREAM_MUSIC,
        44100) != NO_ERROR || host_out_frame_count <= 0) {
        LOGE("cannot compute frame count");
        goto start_err;
    }
    LOGD("reported frame count: output %d", host_out_frame_count);
    
    if (host_track.set(AUDIO_STREAM_MUSIC, 44100, AUDIO_FORMAT_PCM_16_BIT,
            AUDIO_CHANNEL_OUT_STEREO, host_out_frame_count) != NO_ERROR) {
        LOGE("cannot initialize audio device");
        goto start_err;
    }

    LOGD("latency: output %d", host_track.latency());

    size_out = host_out_frame_count;//*host_track.frameSize();

    LOGD("pcm in %d, pcm out %d", size_in, size_out);

    size = (size_in>size_out)?size_out:size_in;

    LOGD("audio track start");
    host_track.start();

    while (capturing) {
        int r_bytes = usb_audio_read(dev_in_pcm, buffer_read, MAX_BUF_SIZE, size);
        LOGD("read bytes: %d", r_bytes);

        if (r_bytes<0)
            break;

        if (send_track_data(&host_track, buffer_read, r_bytes)<0)
            break;
    }

start_err:
    if (dev_in_pcm) usb_audio_close(dev_in_pcm);
    host_track.stop();
    return 0;
}

}

#if 0
static void * sound_in_thread(void *param)
{
    int ret = 0;
    size_t size, size_in, size_out;
    char buffer_read[MAX_BUF_SIZE];
    int host_card = 0;
    int dev_card = 1;
    int device = 0;
    
    struct usb_audio_pcm *dev_in_pcm = NULL;
    struct usb_audio_pcm *host_out_pcm = NULL;
    
    dev_in_pcm = usb_audio_open(dev_card, device, PCM_IN, &pcm_config_ua_in);
    if (!dev_in_pcm) goto start_err;
    host_out_pcm = usb_audio_open(host_card, device, PCM_OUT, &pcm_config_rt5631_out);
    if (!host_out_pcm) goto start_err;

    ret = usb_audio_set_resample(dev_in_pcm, 
                    pcm_config_rt5631_out.rate,
                    pcm_config_rt5631_out.channels, 5);
    if (ret)
    {
        LOGE("resample setup failed\n");
        goto start_err;
    }

    size_in = usb_audio_get_buffer_size(dev_in_pcm);
    size_out = usb_audio_get_buffer_size(host_out_pcm);

    LOGD("pcm in %d, pcm out %d", size_in, size_out);

    size = (size_in>size_out)?size_out:size_in;

    while (capturing) {
        int r_bytes = usb_audio_read(dev_in_pcm, buffer_read, MAX_BUF_SIZE, size);
//        LOGD("read bytes: %d", r_bytes);

        if (r_bytes<0)
            break;

        if (usb_audio_write(host_out_pcm, buffer_read, r_bytes)<0) {
            LOGE("Error playing sample\n");
            break;
        }
    }

start_err:
    if (dev_in_pcm) usb_audio_close(dev_in_pcm);
    if (host_out_pcm) usb_audio_close(host_out_pcm);
    return 0;
}

int start_thread1()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid_dispatch, &attr, sound_in_thread, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

static void * sound_out_thread(void *param)
{
    int ret = 0;
    size_t size, size_in, size_out;
    char buffer_read[MAX_BUF_SIZE];
    int host_card = 0;
    int dev_card = 1;
    int device = 0;
    
    struct usb_audio_pcm *dev_out_pcm = NULL;
    struct usb_audio_pcm *host_in_pcm = NULL;
    
    dev_out_pcm = usb_audio_open(dev_card, device, PCM_OUT, &pcm_config_ua_out);
    if (!dev_out_pcm) goto start_err;
    host_in_pcm = usb_audio_open(host_card, device, PCM_IN, &pcm_config_rt5631_in);
    if (!host_in_pcm) goto start_err;

    ret = usb_audio_set_resample(dev_out_pcm, 
                    pcm_config_rt5631_in.rate,
                    pcm_config_rt5631_in.channels, 5);
    if (ret)
    {
        LOGE("resample setup failed\n");
        goto start_err;
    }

    size_in = usb_audio_get_buffer_size(host_in_pcm);
    size_out = usb_audio_get_buffer_size(dev_out_pcm);

    LOGD("pcm in %d, pcm out %d", size_in, size_out);

    size = (size_in>size_out)?size_out:size_in;

    while (capturing) {
        int r_bytes = usb_audio_read(host_in_pcm, buffer_read, MAX_BUF_SIZE, size);
        LOGD("read bytes: %d", r_bytes);
        
        if (r_bytes<0)
            break;

        if (usb_audio_write(dev_out_pcm, buffer_read, r_bytes)<0) {
            LOGE("Error playing sample\n");
            break;
        }
    }

start_err:
    if (dev_out_pcm) usb_audio_close(dev_out_pcm);
    if (host_in_pcm) usb_audio_close(host_in_pcm);
    return 0;
}

int start_thread2()
{
    int ret;
    pthread_attr_t attr;
    pthread_t tid_dispatch;
    
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid_dispatch, &attr, sound_out_thread, (void*)NULL);

    if (ret < 0) {
        LOGD("Failed to create dispatch thread errno:%d", errno);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    FILE *file;
    struct wav_header header;
    unsigned int card = 0;
    unsigned int device = 0;

    /* install signal handler and begin capturing */
//    signal(SIGINT, sigint_handler);
//    signal(SIGTERM, sigint_handler);

    start_thread1();
//    start_thread2();
    while (capturing) ;

    return 0;
}
#endif

int main(int argc, char **argv)
{
    /* install signal handler and begin capturing */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    sound_in();
//    start_thread1();
//    start_thread2();

    return 0;
}

