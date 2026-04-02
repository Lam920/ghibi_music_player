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

// enum {
//     CMD_NONE,
//     CMD_NEXT,
//     CMD_PREV,
//     CMD_PLAY,
//     CMD_PAUSE,
//     CMD_LIST
// } g_cmd;