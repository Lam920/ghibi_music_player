#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <dirent.h>
#include <linux/fb.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "text_util.h"
#include "rtc.h"

typedef struct {
    char     magic[4];
    uint32_t nframes;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t frame_size;  /* bytes of pixel data per frame */
} __attribute__((packed)) GifHeader;

typedef struct {
    int  index;
    char path[64];
} GhibliGif;

/* ── display mode ──────────────────────────────────────────────────────────*/

typedef enum {
    MODE_GIF      = 0,
    MODE_CALENDAR = 1
} DisplayMode_t;

#define CALENDAR_TICK_NS    (500000000L)        /* redraw clock every 500 ms            */

#endif /* DISPLAY_MANAGER_H */