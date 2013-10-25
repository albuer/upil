/* tinycap.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include "asoundlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

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

struct pcm_config pcm_config_ua_in = {
    .channels = 1,
    .rate = 8000,
    .period_size = 512,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = 0,
    .silence_threshold = 0,
};

struct pcm_config pcm_config_rt5631_in = {
    .channels = 2,
    .rate = 44100,
    .period_size = 2048,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = 0,
    .silence_threshold = 0,
};

int capturing = 1;

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits);

void sigint_handler(int sig)
{
    capturing = 0;
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

#define MAX_BUF_SIZE    128*1024
char buffer_read[MAX_BUF_SIZE];

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits)
{
    struct pcm_config* config = &pcm_config_rt5631_in;
    unsigned int size;
    unsigned int bytes_read = 0;
    struct usb_audio_pcm * uapcm;
    int ret;

    uapcm = usb_audio_open(card, device, PCM_IN, config);

    if (uapcm == NULL)
        return 0;

    ret = usb_audio_set_resample(uapcm, rate, channels, 5);
    if (ret)
    {
        fprintf(stderr, "resample setup failed\n");
    }

    size = usb_audio_get_buffer_size(uapcm);// 4096

    fprintf(stdout, "pcm read size:%d\n", size);

    while (capturing) {
        int r_bytes = usb_audio_read(uapcm, buffer_read, MAX_BUF_SIZE, size);
        
        if (r_bytes<0)
            break;
        
        if (fwrite(buffer_read, 1, r_bytes, file) != r_bytes) {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
        bytes_read += r_bytes;
    }

    usb_audio_close(uapcm);
    return bytes_read / ((bits / 8) * channels);
}

