#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define LOG_NDEBUG 0
#define LOG_TAG "hplay"
#include <utils/Log.h>
#include <media/AudioSystem.h>
#include <media/AudioRecord.h>
#include <media/AudioTrack.h>
#include <system/audio.h>

enum {
    ON_HOLD = 0,
    MUTED = 1,
    NORMAL = 2,
    ECHO_SUPPRESSION = 3,
    LAST_MODE = 3,
};

namespace {

using namespace android;

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

uint16_t applyVolume(uint16_t vol, int16_t* mMixBuffer, int mChannelCount, size_t mFrameCount)
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

    mLeftVolShort = leftVol;
    mRightVolShort = rightVol;

    return mLeftVolShort;
}

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

}

int main(int argc, char **argv)
{
    FILE *file;
    struct wav_header header;
    unsigned int card = 0;
    unsigned int device = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.wav [-d device]\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Unable to open file '%s'\n", argv[1]);
        return 1;
    }

    /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            device = atoi(*argv);
        }
        argv++;
    }

    fread(&header, sizeof(struct wav_header), 1, file);

    if ((header.riff_id != ID_RIFF) ||
        (header.riff_fmt != ID_WAVE) ||
        (header.fmt_id != ID_FMT) ||
        (header.audio_format != FORMAT_PCM) ||
        (header.fmt_sz != 16)) {
        fprintf(stderr, "Error: '%s' is not a PCM riff/wave file\n", argv[1]);
        fclose(file);
        return 1;
    }

    if (header.bits_per_sample != 16)
    {
        fprintf(stderr, "Error: support 16bits only\n");
        fclose(file);
        return 1;
    }

    fprintf(stdout, "Input channel(%d) sample(%d) bits(%d)\n",
            header.num_channels, header.sample_rate, header.bits_per_sample);
    
    fprintf(stdout, "To card(%d) device(%d)\n", card, device);
    
    play_sample(file, card, device, header.num_channels, header.sample_rate,
                header.bits_per_sample);

    fclose(file);

    return 0;
}

