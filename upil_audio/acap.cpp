#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

//#define LOG_NDEBUG 0
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
    rate : 44100,//8000,//
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
int sound_out()
{
    unsigned int bytes_read = 0;
    AudioRecord record;
    char* buffer;
    int frame_count;
    size_t size;

    if (AudioRecord::getMinFrameCount(&frame_count, 8000,
        AUDIO_FORMAT_PCM_16_BIT, 2) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return 0;
    }
    LOGD("reported frame count: %d", frame_count);

    if (record.set(AUDIO_STREAM_VOICE_CALL, 8000, AUDIO_FORMAT_PCM_16_BIT,
        AUDIO_CHANNEL_IN_STEREO, frame_count*2) != NO_ERROR) {
        LOGE("cannot initialize audio device");
        return 0;
    }

    LOGD("latency: %d", record.latency());

    LOGD("record.frameSize %d", record.frameSize());
    size = frame_count*record.frameSize();
    
    buffer = (char*)malloc(size);

    LOGD("record.start");
    record.start();
///    int16_t one;
//    LOGD("Read one");
//    record.read(&one, sizeof(one));

    while (capturing) {
        int r_frame_count = frame_count;
        int chances = 100;
        
        while (--chances > 0 && r_frame_count > 0) {
            AudioRecord::Buffer ar_buffer;
            ar_buffer.frameCount = r_frame_count;
            status_t status = record.obtainBuffer(&ar_buffer, 1);
//            LOGD("obtainBuffer: %d", status);
            if (status == NO_ERROR) {
                int offset = frame_count - r_frame_count;
                LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
                    offset, ar_buffer.frameCount, ar_buffer.size);
                memcpy(&buffer[offset*record.frameSize()], ar_buffer.i8, ar_buffer.size);
                r_frame_count -= ar_buffer.frameCount;
                record.releaseBuffer(&ar_buffer);
            } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
                LOGE("cannot write to AudioTrack");
                break;
            }
        }
        
//        if (fwrite(buffer, 1, size, file) != size) {
//            fprintf(stderr,"Error capturing sample\n");
//            break;
//        }
        bytes_read += size;
    }

    free(buffer);

    record.stop();
    
    return 0;//bytes_read / ((bits / 8) * channels);
}
#endif

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

int recv_record_data(AudioRecord* record, char* data, size_t count)
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
            LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
                offset, buffer.frameCount, buffer.size);
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

static inline int16_t clamp16(int32_t sample)
{
    if ((sample>>15) ^ (sample>>31))
        sample = 0x7FFF ^ (sample>>31);
    return sample;
}

static inline
int32_t mul(int16_t in, int16_t v)
{
#if defined(__arm__) && !defined(__thumb__)
    int32_t out;
    asm( "smulbb %[out], %[in], %[v] \n"
         : [out]"=r"(out)
         : [in]"%r"(in), [v]"r"(v)
         : );
    return out;
#else
    return in * int32_t(v);
#endif
}


uint16_t applyVolume(uint16_t vol, int16_t* mMixBuffer, size_t mFrameCount, int mChannelCount)
{
    size_t frameCount = mFrameCount;
    int16_t *out = mMixBuffer;
    uint16_t leftVol = vol;
    uint16_t rightVol = vol;
    uint16_t mLeftVolShort;
    uint16_t mRightVolShort;

    if (mChannelCount == 1) {
        int32_t d = ((int32_t)leftVol - (int32_t)mLeftVolShort) << 16;
        int32_t vlInc = d / (int32_t)frameCount;
        int32_t vl = ((int32_t)mLeftVolShort << 16);
        do {
            out[0] = clamp16(mul(out[0], vl >> 16) >> 12);
            out++;
            vl += vlInc;
        } while (--frameCount);

    } else {
        int32_t d = ((int32_t)leftVol - (int32_t)mLeftVolShort) << 16;
        int32_t vlInc = d / (int32_t)frameCount;
        d = ((int32_t)rightVol - (int32_t)mRightVolShort) << 16;
        int32_t vrInc = d / (int32_t)frameCount;
        int32_t vl = ((int32_t)mLeftVolShort << 16);
        int32_t vr = ((int32_t)mRightVolShort << 16);
        do {
            out[0] = clamp16(mul(out[0], vl >> 16) >> 12);
            out[1] = clamp16(mul(out[1], vr >> 16) >> 12);
            out += 2;
            vl += vlInc;
            vr += vrInc;
        } while (--frameCount);
    }

    // convert back to unsigned 8 bit after volume calculation
    mLeftVolShort = leftVol;
    mRightVolShort = rightVol;

    return mLeftVolShort;
}

int sound_out()
{
    int ret = 0;
    size_t size, size_in, size_out;
    char buffer_read[MAX_BUF_SIZE];
    int dev_card = 1;
    int device = 0;
    struct usb_audio_pcm *dev_out_pcm = NULL;
    int host_in_frame_count;
    AudioRecord host_record;

    if (AudioRecord::getMinFrameCount(&host_in_frame_count, 44100,
        AUDIO_FORMAT_PCM_16_BIT, 2) != NO_ERROR || host_in_frame_count <= 0) {
        LOGE("cannot compute frame count");
        return 0;
    }
    LOGD("reported frame count: %d", host_in_frame_count);

    if (host_record.set(AUDIO_STREAM_VOICE_CALL, 44100, AUDIO_FORMAT_PCM_16_BIT,
        AUDIO_CHANNEL_IN_STEREO, host_in_frame_count*2) != NO_ERROR) {
        LOGE("cannot initialize audio device");
        return 0;
    }

    LOGD("latency: %d", host_record.latency());

    LOGD("record.frameSize %d", host_record.frameSize());
    size_in = host_in_frame_count*host_record.frameSize();

    dev_out_pcm = usb_audio_open(dev_card, device, PCM_OUT, &pcm_config_ua_out);
    if (!dev_out_pcm) goto start_err;

    ret = usb_audio_set_resample(dev_out_pcm, 
                    pcm_config_rt5631_in.rate,
                    pcm_config_rt5631_in.channels, 5);
    if (ret)
    {
        LOGE("resample setup failed\n");
        goto start_err;
    }

    usb_audio_set_volume(dev_out_pcm, 12);

    size_out = usb_audio_get_buffer_size(dev_out_pcm);

    LOGD("pcm in %d, pcm out %d", size_in, size_out);

    size = (size_in>size_out)?size_out:size_in;

    prepare_record();
    
    LOGD("record.start");
    host_record.start();

    while (capturing) {
        if (recv_record_data(&host_record, buffer_read, size)<0) {
            LOGE("Error receive host record data\n");
            break;
        }

        //applyVolume(0x0100, (int16_t*)buffer_read, size/4, 2);
        if (usb_audio_write(dev_out_pcm, buffer_read, size)<0) {
            LOGE("Error playing sample\n");
            break;
        }
    }

start_err:
    if (dev_out_pcm) usb_audio_close(dev_out_pcm);
    host_record.stop();
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

    sound_out();
    
    return 0;
}

