#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "tinyalsa/pcm.h"

#define ALSA_DEVICE     "hw:0,0"

typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} WAVHeader;

struct gif_music_mapping {
    const char *gif;
    const char *wav;
};

extern struct gif_music_mapping playlist[];

#endif /* MUSIC_PLAYER_H */