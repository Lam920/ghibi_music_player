#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "tinyalsa/pcm.h"

#define ALSA_DEVICE     "hw:0,0"
#define MUSIC_DIR "/opt/ghibli_music"
#define CONFIG_FILE "/opt/config/gif_to_wave.conf"
#define VOLUME_STEP 10
#define VOLUME_MIN  0
#define VOLUME_MAX  100
#define VOLUME_DEFAULT 20

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

/* Playback state structure */
typedef struct {
    volatile sig_atomic_t stop_playback;
    volatile sig_atomic_t running;
    pthread_t             playback_thread;
    pthread_mutex_t       mutex;
    int                   bus_fd;
    volatile int          volume;  /* 0-100 */
} playback_state_t;

extern struct gif_music_mapping *playlist;

#endif /* MUSIC_PLAYER_H */