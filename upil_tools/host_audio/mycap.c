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

#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <audio_utils/resampler.h>

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
    .period_size = 160,
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
    unsigned int channels = 1;
    unsigned int rate = 8000;
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

void to_mono(int16_t* in_buffer, size_t in_count, int16_t* out_buffer, size_t* out_count)
{
    size_t i=0;

    *out_count = 0;
    for (;i<in_count;i+=2)
    {
        out_buffer[(*out_count)++] = in_buffer[i];
    }
}

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits)
{
    struct pcm_config* config = &pcm_config_ua_in;
    struct pcm *pcm;
    char *buffer;
    unsigned int size;
    unsigned int bytes_read = 0;
    struct resampler_itfe *resampler=NULL;
    char out_buffer[128*1024];
    char buffer_mono[128*1024];
    int frame_size = 0;
    int need_resample = 0;

    if (rate != config->rate)
    {
        int ret;
        ret = create_resampler(config->rate,   // in rate
                               rate,        // out rate
                               config->channels,       // channel
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &resampler);
        if (ret != 0)
            return 0;

        need_resample = 1;

    }
    
    frame_size = (bits/8)*config->channels;
    
    pcm = pcm_open(card, device, PCM_IN, config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Unable to open PCM device (%s)\n",
                pcm_get_error(pcm));
        return 0;
    }

    size = pcm_get_buffer_size(pcm)*frame_size;
    
    fprintf(stdout, "In buffer size: %d\n", size);
    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return 0;
    }

    while (capturing && !pcm_read(pcm, buffer, size)) {
        char* in_buf = buffer;
        size_t in_size = size;

        if (need_resample)
        {
            size_t in_frames,out_frames;
            in_frames = size/frame_size;
            out_frames = 128*1024/frame_size;
            
            resampler->resample_from_input(resampler,
                                            (int16_t *)buffer,
                                            &in_frames,
                                            (int16_t *)out_buffer,
                                            &out_frames);
            fprintf(stdout, "resample, in %d, out %d\n", in_frames, out_frames);
            in_buf = out_buffer;
            in_size = out_frames*frame_size;
        }

        if (channels==1)
        {
            size_t frames_mono = 0;
            fprintf(stdout, "to_mono\n");
            to_mono((int16_t *)in_buf, in_size/sizeof(int16_t),
                            (int16_t *)buffer_mono, &frames_mono);
            in_buf = buffer_mono;
            in_size = frames_mono*sizeof(int16_t);
        }

        if (fwrite(in_buf, 1, in_size, file) != in_size) {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
        bytes_read += in_size;
    }

    free(buffer);
    if (resampler)
        release_resampler(resampler);
    
    pcm_close(pcm);
    return bytes_read / ((bits / 8) * channels);
}

