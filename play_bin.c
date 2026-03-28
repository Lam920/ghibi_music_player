#include "play_bin.h"


#define FB_DEV     "/dev/fb0"
#define MIN_FPS    25
#define MAGIC      "ANIM"

#define GLIBLI_GIF_DIR "/opt/ghibli_gif"


struct ghibli_gif *ghibli_gif_head = NULL;
struct ghibli_gif *ghibli_gif_tail = NULL;

static void get_ghibli_gifs()
{
    DIR *dir;
    struct dirent *entry;
    int index = 0;

    dir = opendir(GLIBLI_GIF_DIR);
    if (!dir) {
        perror("opendir");
        return; 
    }

    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; 
        }

        struct ghibli_gif *new_gif = malloc(sizeof(struct ghibli_gif));
        if (!new_gif) {
            perror("malloc");
            closedir(dir);
            return;
        }

        new_gif->index = index++;
        snprintf(new_gif->path, sizeof(new_gif->path), "%s/%s", GLIBLI_GIF_DIR, entry->d_name);
        new_gif->next = NULL;
        if (!ghibli_gif_head) {
            ghibli_gif_head = new_gif;
        } else {
            ghibli_gif_tail->next = new_gif;
        }
        ghibli_gif_tail = new_gif;

    }

    closedir(dir);
}

static void free_ghibli_gifs()
{
    struct ghibli_gif *current = ghibli_gif_head;
    while (current) {
        struct ghibli_gif *next = current->next;
        free(current);
        current = next;
    }
    ghibli_gif_head = NULL;
    ghibli_gif_tail = NULL;
}

static void print_ghibli_gifs()
{
    struct ghibli_gif *current = ghibli_gif_head;
    while (current) {
        printf("GIF %d: %s\n", current->index, current->path);
        current = current->next;
    }
}


int main(int argc, char *argv[])
{
    int                      fd;
    struct stat              st;
    const uint8_t           *data;
    const GifHeader         *hdr;
    uint32_t                 frame_stride;
    const uint8_t           *frames_start;
    struct fb_fix_screeninfo finfo;
    int                      fbfd;
    uint8_t                 *fbmem;
    long                     min_ns;
    struct timespec          next;
    uint32_t                 i;

    const uint8_t *fp;
    uint16_t       delay_ms;
    long           delay_ns;

    /* Get list of Ghibli GIFs */
    get_ghibli_gifs();
    print_ghibli_gifs();

    /* ── open .bin via mmap ───────────────────────── */
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open bin"); return 1; }

    fstat(fd, &st);

    /* mmap entire file — kernel streams pages on demand, no RAM spike */
    data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { perror("mmap bin"); return 1; }

    /* ── validate header ──────────────────────────── */
    hdr = (const GifHeader *)data;
    if (memcmp(hdr->magic, MAGIC, 4) != 0) {
        fprintf(stderr, "Invalid file format\n"); return 1;
    }
    printf("Animation: %ux%u, %u frames, %u fps\n",
           hdr->width, hdr->height, hdr->nframes, hdr->fps);

    /* Extract GIF data from header */
    frame_stride  = sizeof(uint16_t) + hdr->frame_size;
    frames_start  = data + sizeof(GifHeader);

    /* ── open framebuffer ─────────────────────────── */
    fbfd = open(FB_DEV, O_RDWR);
    if (fbfd < 0) { perror("open fb"); return 1; }
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);

    fbmem = mmap(NULL, finfo.smem_len,
                 PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) { perror("mmap fb"); return 1; }

    /* ── hint kernel: we'll stream sequentially ──── */
    madvise((void*)data, st.st_size, POSIX_MADV_SEQUENTIAL);

    /* ── play loop ────────────────────────────────── */
    min_ns = 1000000000L / MIN_FPS;
    clock_gettime(CLOCK_MONOTONIC, &next);

    printf("Playing — Ctrl+C to stop\n");

    for (i = 0; ; i = (i + 1) % hdr->nframes) {

        fp = frames_start + i * frame_stride;

        /* First 2 bytes of each frame: delay in ms */
        memcpy(&delay_ms, fp, sizeof(uint16_t));

        /* zero-copy: frame pixels sit directly in mmap'd file */
        memcpy(fbmem, fp + sizeof(uint16_t), hdr->frame_size);

        delay_ns = delay_ms * 1000000L;
        if (delay_ns < min_ns) delay_ns = min_ns;

        /* For sleeping exactly until the next frame ~ due to each frame's delay is variable
        e.g. 100ms for slow frames, 10ms for fast ones
        */
        next.tv_nsec += delay_ns;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return 0;
}