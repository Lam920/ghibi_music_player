#include "play_bin.h"

#define FB_DEV         "/dev/fb0"
#define MIN_FPS        25
#define MAGIC          "ANIM"
#define GHIBLI_GIF_DIR "/opt/ghibli_gif"
#define MAX_GIFS       64

const char *SOCKNAME = "/tmp/ghibli_gif.sock";

/* Array-based GIF storage for O(1) access */
static GhibliGif gif_array[MAX_GIFS];
static int       gif_count = 0;

/* UDS socket fds: Server and Client fd */
static int g_sockfd = -1;
static int g_connfd = -1;

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

static void cleanup_socket(void)
{
    if (g_connfd >= 0) 
    { 
        close(g_connfd); 
        g_connfd = -1; 
    }
    if (g_sockfd >= 0) 
    { 
        close(g_sockfd); 
        g_sockfd = -1; 
    }
    unlink(SOCKNAME);
}

/*  UDS socket init (non-blocking) */

static void init_socket(void)
{
    struct sockaddr_un addr;
    int flags;

    unlink(SOCKNAME); /* remove stale socket if exists */

    g_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_sockfd < 0) 
    { 
        perror("socket"); 
        exit(1); 
    }

    flags = fcntl(g_sockfd, F_GETFL, 0);
    fcntl(g_sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKNAME, sizeof(addr.sun_path) - 1);

    if (bind(g_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); 
        cleanup_socket(); 
        exit(1);
    }
    if (listen(g_sockfd, 5) < 0) {
        perror("listen"); 
        cleanup_socket(); 
        exit(1);
    }

    printf("Control socket: %s\n", SOCKNAME);
    printf("Commands: PAUSE | RESUME | TOGGLE | NEXT | PREV | PLAY <name> | LIST\n");
}

/*  command handler */

static void sock_write(const char *msg)
{
    ssize_t written = write(g_connfd, msg, strlen(msg));
    (void)written; /* best-effort; disconnect caught on next recv */
}

static void handle_command(const char *cmd)
{
    if (strcmp(cmd, "PAUSE") == 0) {
        g_paused = 1;
        sock_write("OK\n");

    } else if (strcmp(cmd, "RESUME") == 0) {
        g_paused = 0;
        sock_write("OK\n");

    } else if (strcmp(cmd, "TOGGLE") == 0) {
        g_paused ^= 1;
        sock_write("OK\n");

    } else if (strcmp(cmd, "NEXT") == 0) {
        if (g_cmd != CMD_NONE) {
            sock_write("BUSY\n");
            return;
        }
        g_paused = 0;
        g_cmd = CMD_NEXT;
        sock_write("OK\n");

    } else if (strcmp(cmd, "PREV") == 0) {
        if (g_cmd != CMD_NONE) {
            sock_write("BUSY\n");
            return;
        }
        g_paused = 0;
        g_cmd = CMD_PREV;
        sock_write("OK\n");

    } else if (strncmp(cmd, "PLAY ", 5) == 0) {
        if (g_cmd != CMD_NONE) {
            sock_write("BUSY\n");
            return;
        }
        const char *name = cmd + 5;
        if (check_gif_exists(name)) {
            memset(g_play_name, 0, sizeof(g_play_name));
            strncpy(g_play_name, name, sizeof(g_play_name) - 1);
            g_play_name[sizeof(g_play_name) - 1] = '\0';
            g_paused = 0;
            g_cmd = CMD_PLAY;
            sock_write("OK\n");
        } else {
            sock_write("ERR: GIF not found\n");
        }

    } else if (strcmp(cmd, "LIST") == 0) {
        int j;
        for (j = 0; j < gif_count; j++) {
            char line[80];
            snprintf(line, sizeof(line), "%d:%s\n",
                     gif_array[j].index, gif_array[j].path);
            sock_write(line);
        }
        sock_write("OK\n");

    } else {
        sock_write("ERR: unknown command\n");
    }
}

/*  non-blocking socket poll (called each frame) */

static void poll_socket(void)
{
    char    buf[128];
    ssize_t n;
    int     flags;

    /* Accept a new connection if none active */
    if (g_connfd < 0) {
        g_connfd = accept(g_sockfd, NULL, NULL);
        if (g_connfd >= 0) 
        {
            flags = fcntl(g_connfd, F_GETFL, 0);
            fcntl(g_connfd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    if (g_connfd < 0) 
        return;

    n = recv(g_connfd, buf, sizeof(buf) - 1, 0);
    if (n < 0) 
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) 
            return;
        close(g_connfd); 
        g_connfd = -1;
        return;
    }
    if (n == 0) 
    { 
        /* client disconnected */
        close(g_connfd); 
        g_connfd = -1;
        return;
    }

    buf[n] = '\0';
    /* Strip trailing CR/LF/space */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = '\0';

    handle_command(buf);
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

    /* init UDS socket */
    init_socket();

    /* open framebuffer once (reused across GIF switches) */
    fbfd = open(FB_DEV, O_RDWR);
    if (fbfd < 0) { 
        perror("open fb"); 
        cleanup_socket(); 
        return 1; 
    }
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    fbmem = mmap(NULL, finfo.smem_len,
                 PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbmem == MAP_FAILED) 
    {
        perror("mmap fb");
        close(fbfd);
        cleanup_socket();
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

        printf("Playing: %s (%ux%u, %u frames @ %u fps)\n",
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

            poll_socket();

            /* Pause: spin with 16 ms sleep, keep polling socket */
            while (g_paused && g_running && g_cmd == CMD_NONE) \
            {
                poll_socket();
                struct timespec ts = {0, 16000000L};
                nanosleep(&ts, NULL);
            }

            if (!g_running || g_cmd != CMD_NONE) 
                break;

            fp = frames_start + i * frame_stride;
            memcpy(&delay_ms, fp, sizeof(uint16_t));
            memcpy(fbmem, fp + sizeof(uint16_t), hdr->frame_size);

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
        } else if (g_cmd == CMD_PREV) 
        {
            current_idx = (current_idx - 1 + gif_count) % gif_count;
        } else if (g_cmd == CMD_PLAY) 
        {
            int j;
            for (j = 0; j < gif_count; j++) {
                if (strcmp(gif_array[j].path, g_play_name) == 0) {
                    current_idx = j;
                    break;
                }
            }
        }
        g_cmd = CMD_NONE;
    }

    /* cleanup */
    munmap(fbmem, finfo.smem_len);
    close(fbfd);
    cleanup_socket();

    return 0;
}