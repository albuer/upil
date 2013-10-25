/*
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

void play_sample(FILE *file, unsigned int card, unsigned int device, 
                 unsigned int channels, unsigned int rate, unsigned int bits);

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

#define MAX_BUFFER_SIZE     128*1024

void to_stereo(int16_t* in_buffer, size_t in_count, int16_t* out_buffer, size_t* out_count)
{
    size_t i=0;

    *out_count = 0;
    for (;i<in_count;i++)
    {
        out_buffer[(*out_count)++] = in_buffer[i];
        out_buffer[(*out_count)++] = in_buffer[i];
    }
}

struct pcm_config pcm_config_ua_out = {
    .channels = 2,
    .rate = 48000,
    .period_size = 1024,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = 0,
    .silence_threshold = 0,
};

void play_sample(FILE *file, unsigned int card, unsigned int device, 
                 unsigned int channels, unsigned int rate, unsigned int bits)
{
    struct pcm_config* config = &pcm_config_ua_out;
    struct pcm *pcm;
    char *buffer;
    int size;
    int num_read;

    struct resampler_itfe *resampler=NULL;
    char out_buffer[MAX_BUFFER_SIZE];
    char buffer_stereo[MAX_BUFFER_SIZE];
    
    int frame_size = 0;
    int need_resample = 0;

    if (rate != config->rate)
    {
        int ret;
        ret = create_resampler(rate,   // in rate
                               config->rate,      // out rate
                               config->channels,  // channel
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &resampler);
        if (ret != 0)
            return;

        need_resample = 1;
        
    }

    frame_size = (bits/8)*config->channels;

    pcm = pcm_open(card, device, PCM_OUT, config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        return;
    }

    size = pcm_get_buffer_size(pcm);
    fprintf(stdout, "pcm buffer size:%d\n", size);
    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return;
    }

    do {
        num_read = fread(buffer, 1, size, file);
        if (num_read > 0) {
            char* in_buf = buffer;
            size_t in_size = num_read;
            
            if (channels==1)
            {
                size_t frames_stereo = 0;
                fprintf(stdout, "to_stereo\n");
                to_stereo((int16_t *)in_buf, in_size/sizeof(int16_t),
                                (int16_t *)buffer_stereo, &frames_stereo);
                in_buf = buffer_stereo;
                in_size = frames_stereo*sizeof(int16_t);
            }

            if (need_resample)
            {
                size_t in_frames,out_frames;
                fprintf(stdout, "resample\n");
                in_frames = in_size/frame_size;
                out_frames = MAX_BUFFER_SIZE/frame_size;
                
                resampler->resample_from_input(resampler,
                                                (int16_t *)in_buf,
                                                &in_frames,
                                                (int16_t *)out_buffer,
                                                &out_frames);
                in_buf = out_buffer;
                in_size = out_frames*frame_size;
            }

            if (pcm_write(pcm, in_buf, in_size)) {
                fprintf(stderr, "Error playing sample\n");
                break;
            }
        }
    } while (num_read > 0);

    if (resampler)
        release_resampler(resampler);

    free(buffer);
    pcm_close(pcm);
}

