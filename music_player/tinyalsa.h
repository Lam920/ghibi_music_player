#ifndef TINYALSA_H
#define TINYALSA_H

#include <stdint.h>
#include <sys/time.h>
#include <limits.h>

#define PCM_OUT 0x00000000
#define PCM_IN  0x10000000

struct pcm_config {
    unsigned int channels;
    unsigned int rate;
    unsigned int period_size;
    unsigned int period_count;
    unsigned int format;
    unsigned int start_threshold;
    unsigned int stop_threshold;
    unsigned int silence_threshold;
    int avail_min;
};

enum pcm_format {
    PCM_FORMAT_S16_LE = 0,
    PCM_FORMAT_S32_LE,
    PCM_FORMAT_S8,
    PCM_FORMAT_S24_LE,
    PCM_FORMAT_S24_3LE,
};

struct pcm;

struct pcm *pcm_open(unsigned int card, unsigned int device,
                     unsigned int flags, struct pcm_config *config);
int pcm_close(struct pcm *pcm);
int pcm_is_ready(struct pcm *pcm);
int pcm_write(struct pcm *pcm, const void *data, unsigned int count);
const char *pcm_get_error(struct pcm *pcm);

#endif /* TINYALSA_H */
