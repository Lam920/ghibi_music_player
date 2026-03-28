#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <dirent.h>
#include <linux/fb.h>

typedef struct {
    char     magic[4];
    uint32_t nframes;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t frame_size;  /* bytes of pixel data per frame */
} __attribute__((packed)) GifHeader;


struct ghibli_gif {
    int index;
    char path[64];
    struct ghibli_gif *next;
};

extern struct ghibli_gif *ghibli_gif_head;
extern struct ghibli_gif *ghibli_gif_tail;