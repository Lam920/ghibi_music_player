#include "play_gif.h"
#include "../IPC/bus_client.h"
#include "../IPC/ipc_protocol.h"

#define FB_DEV         "/dev/fb0"
#define MIN_FPS        25
#define MAGIC          "ANIM"
#define GHIBLI_GIF_DIR "/opt/ghibli_gif"
#define MAX_GIFS       64

/* Color format: Enable for BGR565 displays (e.g., ST7789 1.54" 240x240) */
#ifndef USE_BGR565
#define USE_BGR565     0  /* Set to 1 for BGR565 displays, 0 for RGB565 (standard) */
#endif

/* Array-based GIF storage for O(1) access */
static GhibliGif gif_array[MAX_GIFS];
static int       gif_count = 0;

/* Event bus connection */
static int g_bus_fd = -1;

/* Commands */
#define CMD_NONE  0
#define CMD_NEXT  1
#define CMD_PREV  2
#define CMD_PLAY  3

static volatile sig_atomic_t g_cmd     = CMD_NONE;
static volatile sig_atomic_t g_paused  = 0;
static volatile sig_atomic_t g_running = 1;
static char                  g_play_name[64];

/* ── GIF array helpers ─*/

static void get_ghibli_gifs(void)
{
    DIR           *dir;
    struct dirent *entry;

    dir = opendir(GHIBLI_GIF_DIR);
    if (!dir) {
        perror("opendir");
        return;
    }

    gif_count = 0;
    while ((entry = readdir(dir)) != NULL && gif_count < MAX_GIFS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        gif_array[gif_count].index = gif_count;
        snprintf(gif_array[gif_count].path, sizeof(gif_array[gif_count].path),
                 "%s", entry->d_name);
        gif_count++;
    }
    closedir(dir);
}

static void print_ghibli_gifs(void)
{
    int i;
    for (i = 0; i < gif_count; i++) {
        printf("  [%d] %s\n", gif_array[i].index, gif_array[i].path);
    }
}

static bool check_gif_exists(const char *name)
{
    int i;
    for (i = 0; i < gif_count; i++) {
        if (strcmp(gif_array[i].path, name) == 0)
            return true;
    }
    return false;
}

static int find_gif_index(const char *name)
{
    int i;
    for (i = 0; i < gif_count; i++) {
        if (strcmp(gif_array[i].path, name) == 0)
            return i;
    }
    return -1;
}

/* ── signal & cleanup ────*/

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void cleanup_bus(void)
{
    if (g_bus_fd >= 0) {
        bus_disconnect(g_bus_fd);
        g_bus_fd = -1;
    }
}

/*  Event bus init (non-blocking) */

static void init_event_bus(void)
{
    int flags;

    /* Connect to event bus */
    g_bus_fd = bus_connect(5);
    if (g_bus_fd < 0) {
        fprintf(stderr, "Failed to connect to event bus\n");
        exit(1);
    }

    /* Make it non-blocking for polling */
    flags = fcntl(g_bus_fd, F_GETFL, 0);
    fcntl(g_bus_fd, F_SETFL, flags | O_NONBLOCK);

    /* Subscribe to button events */
    uint64_t mask = EVT_MASK(EVT_BTN_UP) | EVT_MASK(EVT_BTN_DOWN);
    if (bus_subscribe(g_bus_fd, mask) < 0) {
        fprintf(stderr, "Failed to subscribe to events\n");
        cleanup_bus();
        exit(1);
    }

    printf("[play_gif] Connected to event bus\n");
    printf("[play_gif] Subscribed to button events (UP=next, DOWN=prev)\n");
}

/*  Event handler - process button events */

static void handle_event(const ipc_event_t *evt)
{
    switch (evt->type) {
        case EVT_BTN_UP:
            /* Button UP = Next GIF */
            if (g_cmd == CMD_NONE) {
                printf("[play_gif] Button UP -> NEXT\n");
                g_paused = 0;
                g_cmd = CMD_NEXT;
            }
            break;

        case EVT_BTN_DOWN:
            /* Button DOWN = Previous GIF */
            if (g_cmd == CMD_NONE) {
                printf("[play_gif] Button DOWN -> PREV\n");
                g_paused = 0;
                g_cmd = CMD_PREV;
            }
            break;

        default:
            /* Ignore other events */
            break;
    }
}

/*  Non-blocking event bus poll (called each frame) */

static void poll_events(void)
{
    ipc_event_t evt;
    ssize_t n;

    if (g_bus_fd < 0)
        return;

    /* Try to receive an event (non-blocking) */
    n = recv(g_bus_fd, &evt, sizeof(evt), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;  /* No events available */
        /* Connection error */
        fprintf(stderr, "[play_gif] Lost connection to event bus\n");
        cleanup_bus();
        g_running = 0;
        return;
    }

    if (n == 0) {
        /* Bus closed */
        fprintf(stderr, "[play_gif] Event bus closed connection\n");
        cleanup_bus();
        g_running = 0;
        return;
    }

    if ((size_t)n != sizeof(evt)) {
        fprintf(stderr, "[play_gif] Incomplete event received\n");
        return;
    }

    if (evt.magic != IPC_MAGIC) {
        fprintf(stderr, "[play_gif] Invalid event magic\n");
        return;
    }

    /* Process the event */
    handle_event(&evt);
}

/*  Publish GIF changed event */

static void publish_gif_changed(int idx)
{
    ipc_event_t evt;
    bus_evt_init(&evt, EVT_GIF_CHANGED);

    gif_changed_payload_t *payload = EVT_PAYLOAD(&evt, gif_changed_payload_t);
    payload->gif_index = idx;
    strncpy(payload->gif_name, gif_array[idx].path, sizeof(payload->gif_name) - 1);
    payload->gif_name[sizeof(payload->gif_name) - 1] = '\0';
    evt.payload_len = sizeof(gif_changed_payload_t);

    if (bus_publish(g_bus_fd, &evt) == 0) {
        printf("[play_gif] Published EVT_GIF_CHANGED: %s (idx=%d)\n",
               payload->gif_name, idx);
    } else {
        fprintf(stderr, "[play_gif] Failed to publish GIF change event\n");
    }
}

int main(int argc, char *argv[])
{
    struct fb_fix_screeninfo finfo;
    int                      fbfd;
    uint8_t                 *fbmem;
    long                     min_ns;
    struct timespec          next;
    int                      current_idx;
    struct sigaction         sa;

    /* ── discover GIFs ──*/
    get_ghibli_gifs();

    if (gif_count == 0) {
        fprintf(stderr, "No GIFs found in %s\n", GHIBLI_GIF_DIR);
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name>\n", argv[0]);
        print_ghibli_gifs();
        return 1;
    }

    if (!check_gif_exists(argv[1])) {
        fprintf(stderr, "GIF '%s' not found. Available:\n", argv[1]);
        print_ghibli_gifs();
        return 1;
    }
    else {
        memcpy(argv[1], gif_array[0].path, sizeof(gif_array[0].path));
        printf("Starting with GIF: %s\n", argv[1]);
    }

    /* Find starting index */
    current_idx = find_gif_index(argv[1]);
    if (current_idx < 0) {
        fprintf(stderr, "GIF '%s' not found in array. This should not happen.\n", argv[1]);
        return 1;
    }

    /* signal setup */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* init event bus */
    init_event_bus();

    /* Publish initial GIF */
    publish_gif_changed(current_idx);

    /* open framebuffer once (reused across GIF switches) */
    fbfd = open(FB_DEV, O_RDWR);
    if (fbfd < 0) { 
        perror("open fb"); 
        cleanup_bus(); 
        return 1; 
    }
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    fbmem = mmap(NULL, finfo.smem_len,
                 PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbmem == MAP_FAILED) 
    {
        perror("mmap fb");
        close(fbfd);
        cleanup_bus();
        return 1;
    }

    min_ns = 1000000000L / MIN_FPS;

    /* outer loop: load + play one GIF, switch on command */
    while (g_running) {
        int             fd;
        struct stat     st;
        const uint8_t  *data;
        const GifHeader *hdr;
        uint32_t        frame_stride;
        const uint8_t  *frames_start;
        char            gif_path[256];
        uint32_t        i;

        memset(gif_path, 0, sizeof(gif_path));
        snprintf(gif_path, sizeof(gif_path), "%s/%s",
                 GHIBLI_GIF_DIR, gif_array[current_idx].path);

        fd = open(gif_path, O_RDONLY);
        if (fd < 0) 
        { 
            perror("open bin"); 
            break; 
        }

        fstat(fd, &st);
        data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) 
        { 
            perror("mmap bin"); 
            close(fd); 
            break; 
        }

        hdr = (const GifHeader *)data;
        if (memcmp(hdr->magic, MAGIC, 4) != 0) 
        {
            fprintf(stderr, "Invalid format: %s\n", gif_path);
            munmap((void *)data, st.st_size);
            close(fd);
            current_idx = (current_idx + 1) % gif_count;
            continue;
        }

        printf("[play_gif] Playing: %s (%ux%u, %u frames @ %u fps)\n",
               gif_array[current_idx].path,
               hdr->width, hdr->height, hdr->nframes, hdr->fps);
        madvise((void *)data, st.st_size, POSIX_MADV_SEQUENTIAL);

        frame_stride  = sizeof(uint16_t) + hdr->frame_size;
        frames_start  = data + sizeof(GifHeader);

        g_cmd = CMD_NONE;
        clock_gettime(CLOCK_MONOTONIC, &next);

        /* inner loop: render frames */
        for (i = 0; g_running && g_cmd == CMD_NONE; i = (i + 1) % hdr->nframes) 
        {
            const uint8_t *fp;
            uint16_t       delay_ms;
            long           delay_ns;

            poll_events();

            /* Pause: spin with 16 ms sleep, keep polling events, check when gif is paused */
            while (g_paused && g_running && g_cmd == CMD_NONE) 
            {
                poll_events();
                struct timespec ts = {0, 16000000L};
                nanosleep(&ts, NULL);
            }

            if (!g_running || g_cmd != CMD_NONE) 
                break;

            fp = frames_start + i * frame_stride;
            memcpy(&delay_ms, fp, sizeof(uint16_t));
            memcpy(fbmem, fp + sizeof(uint16_t), hdr->frame_size);

#if USE_BGR565
            /* Convert RGB565 to BGR565 for displays with swapped color channels */
            convert_frame_to_bgr((uint16_t *)fbmem, hdr->width * hdr->height);
#endif
            delay_ns = delay_ms * 1000000L;
            if (delay_ns < min_ns) delay_ns = min_ns;

            next.tv_nsec += delay_ns;
            while (next.tv_nsec >= 1000000000L) 
            {
                next.tv_nsec -= 1000000000L;
                next.tv_sec++;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        }

        munmap((void *)data, st.st_size);
        close(fd);

        /* apply switch command */
        if (g_cmd == CMD_NEXT) 
        {
            current_idx = (current_idx + 1) % gif_count;
            publish_gif_changed(current_idx);
        } else if (g_cmd == CMD_PREV) 
        {
            current_idx = (current_idx - 1 + gif_count) % gif_count;
            publish_gif_changed(current_idx);
        } else if (g_cmd == CMD_PLAY) 
        {
            int j;
            for (j = 0; j < gif_count; j++) {
                if (strcmp(gif_array[j].path, g_play_name) == 0) {
                    current_idx = j;
                    publish_gif_changed(current_idx);
                    break;
                }
            }
        }
        g_cmd = CMD_NONE;
    }

    /* cleanup */
    munmap(fbmem, finfo.smem_len);
    close(fbfd);
    cleanup_bus();

    printf("[play_gif] Exiting\n");
    return 0;
}