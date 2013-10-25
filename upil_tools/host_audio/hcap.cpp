#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define LOG_NDEBUG 0
#define LOG_TAG "hcap"
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

int capturing = 1;

void sigint_handler(int sig)
{
    capturing = 0;
}

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


unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits)
{
    unsigned int bytes_read = 0;
    AudioRecord record;
    char* buffer;
    int frame_count;
    size_t size;

    {
        AudioTrack track;
        int t_frame_count;
        char t_buffer[16384];
        if (AudioTrack::getMinFrameCount(&t_frame_count, AUDIO_STREAM_MUSIC,
            44100) != NO_ERROR || t_frame_count <= 0) {
            LOGE("cannot compute frame count");
            return 0;
        }
    
        LOGD("reported frame count: output %d", t_frame_count);
        if (track.set(AUDIO_STREAM_MUSIC, 44100, AUDIO_FORMAT_PCM_16_BIT,
                AUDIO_CHANNEL_OUT_STEREO, t_frame_count) != NO_ERROR) {
            LOGE("cannot initialize audio device");
            return 0;
        }
        LOGD("track.start");
        track.start();

        int i=0;
        for(i=0; i<100; i++) {
            if (track.muted())
                LOGD("muted");
        }
        track.mute(true);
        
        for (i=0; i<5; i++) {
            int num_read = 16384;
            int w_frame_count = num_read/track.frameSize();
            frame_count = w_frame_count;
            int chances = 100;
            
            LOGD("num_read %d, frameSize %d, w_frame_count %d", num_read, 
                track.frameSize(), w_frame_count);
            
        while (--chances > 0 && w_frame_count > 0) {
            AudioTrack::Buffer at_buffer;
            at_buffer.frameCount = w_frame_count;
            status_t status = track.obtainBuffer(&at_buffer, 1);
            LOGD("status %d", status);
            if (status == NO_ERROR) {
                int offset = frame_count - w_frame_count;
                LOGD("offset=%d, buffer.framecount=%d, buffer.size=%d", 
                    offset, at_buffer.frameCount, at_buffer.size);
                memcpy(at_buffer.i8, &t_buffer[offset*track.frameSize()], at_buffer.size);
                w_frame_count -= at_buffer.frameCount;
                track.releaseBuffer(&at_buffer);
            } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
                LOGE("cannot write to AudioTrack");
                break;
            }
        }
            }
    }

    if (AudioRecord::getMinFrameCount(&frame_count, 44100,
        AUDIO_FORMAT_PCM_16_BIT, 2) != NO_ERROR || frame_count <= 0) {
        LOGE("cannot compute frame count");
        return 0;
    }
    LOGD("reported frame count: %d", frame_count);

    if (record.set(AUDIO_STREAM_VOICE_CALL, 44100, AUDIO_FORMAT_PCM_16_BIT,
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
        
        if (fwrite(buffer, 1, size, file) != size) {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
        bytes_read += size;
    }

    free(buffer);

    record.stop();
    
    return bytes_read / ((bits / 8) * channels);
}

}

int main(int argc, char **argv)
{
    FILE *file;
    struct wav_header header;
    unsigned int card = 0;
    unsigned int device = 0;
    unsigned int channels = 2;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    unsigned int frames;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.wav [-d device] [-c channels] "
                "[-r rate] [-b bits]\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "wb");
    if (!file) {
        fprintf(stderr, "Unable to create file '%s'\n", argv[1]);
        return 1;
    }

    /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            device = atoi(*argv);
        } else if (strcmp(*argv, "-c") == 0) {
            argv++;
            channels = atoi(*argv);
        } else if (strcmp(*argv, "-r") == 0) {
            argv++;
            rate = atoi(*argv);
        } else if (strcmp(*argv, "-b") == 0) {
            argv++;
            bits = atoi(*argv);
        }
        argv++;
    }

    header.riff_id = ID_RIFF;
    header.riff_sz = 0;
    header.riff_fmt = ID_WAVE;
    header.fmt_id = ID_FMT;
    header.fmt_sz = 16;
    header.audio_format = FORMAT_PCM;
    header.num_channels = channels;
    header.sample_rate = rate;
    header.bits_per_sample = bits;
    header.byte_rate = (header.bits_per_sample / 8) * channels * rate;
    header.block_align = channels * (header.bits_per_sample / 8);
    header.data_id = ID_DATA;

    /* leave enough room for header */
    fseek(file, sizeof(struct wav_header), SEEK_SET);

    /* install signal handler and begin capturing */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    fprintf(stdout, "Output channel(%d) sample(%d) bits(%d)\n",
            header.num_channels, header.sample_rate, header.bits_per_sample);
    
    fprintf(stdout, "from card(%d) device(%d)\n", card, device);
    
    frames = capture_sample(file, card, device, header.num_channels,
                            header.sample_rate, header.bits_per_sample);
    printf("Captured %d frames\n", frames);

    /* write header now all information is known */
    header.data_sz = frames * header.block_align;
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct wav_header), 1, file);

    fclose(file);

    return 0;
}


