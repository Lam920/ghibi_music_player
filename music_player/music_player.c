#include "music_player.h"
#include "../IPC/bus_client.h"
#include "../IPC/ipc_protocol.h"
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#define MUSIC_DIR "/opt/ghibli_music"

struct gif_music_mapping playlist[] = {
    {"totoro", "porco_rosso.wav"},
    {"haku", "haku.wav"},
    {"sophie", "sophie.wav"},
    {"sosuke", "sosuke.wav"},
    {"solider", "soldier.wav"},
    {NULL, NULL}
};

/* Playback state structure */
typedef struct {
    volatile sig_atomic_t stop_playback;
    volatile sig_atomic_t running;
    pthread_t             playback_thread;
    pthread_mutex_t       mutex;
    int                   bus_fd;
} playback_state_t;

static playback_state_t g_state = {
    .stop_playback = 0,
    .running = 1,
    .playback_thread = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .bus_fd = -1
};


/* Find WAV file for a given GIF name */
static const char *find_wav_for_gif(const char *gif_name) {
    int i;
    for (i = 0; playlist[i].gif != NULL; i++) {
        if (strcmp(playlist[i].gif, gif_name) == 0) {
            return playlist[i].wav;
        }
    }
    return NULL;
}

/* Play WAV file with stop flag checking */
static int play_wav(const char *filename) {
    struct pcm_config config;
    struct pcm *pcm = NULL;
    int buffer_size = 4096;
    char *buffer = NULL;
    size_t bytes_read;
    char chunk_id[4];
    uint32_t chunk_size;
    WAVHeader header;
    int ret = -1;
    char filepath[256];

    snprintf(filepath, sizeof(filepath), "%s/%s", MUSIC_DIR, filename);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "[music_player] Error: Cannot open file %s\n", filepath);
        return -1;
    }

    if (fread(&header, sizeof(WAVHeader), 1, fp) != 1) {
        fprintf(stderr, "[music_player] Error: Cannot read WAV header\n");
        fclose(fp);
        return -1;
    }

    // Verify WAV format
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        fprintf(stderr, "[music_player] Error: Not a valid WAV file\n");
        fclose(fp);
        return -1;
    }

    // Find data chunk
    while (fread(chunk_id, 4, 1, fp) == 1) {
        fread(&chunk_size, 4, 1, fp);
        if (strncmp(chunk_id, "data", 4) == 0) {
            break;
        }
        fseek(fp, chunk_size, SEEK_CUR);
    }

    printf("[music_player] Playing: %s\n", filepath);
    printf("[music_player] Channels: %d, Sample Rate: %d Hz, Bits: %d\n",
           header.num_channels, header.sample_rate, header.bits_per_sample);

    // Configure TinyALSA
    memset(&config, 0, sizeof(config));
    config.channels = header.num_channels;
    config.rate = header.sample_rate;
    config.period_size = 1024;
    config.period_count = 4;
    config.format = (header.bits_per_sample == 16) ? PCM_FORMAT_S16_LE : PCM_FORMAT_S8;

    // Open PCM device (card 0, device 0)
    pcm = pcm_open(0, 0, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "[music_player] Error: Cannot open PCM device (%s)\n", 
                pcm ? pcm_get_error(pcm) : "failed to allocate");
        if (pcm) pcm_close(pcm);
        fclose(fp);
        return -1;
    }

    buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "[music_player] Error: Cannot allocate buffer\n");
        pcm_close(pcm);
        fclose(fp);
        return -1;
    }

    // Playback loop with stop flag checking
    while (!g_state.stop_playback && (bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
        if (pcm_write(pcm, buffer, bytes_read) < 0) {
            fprintf(stderr, "[music_player] Error: Write to PCM device failed\n");
            break;
        }
    }

    if (g_state.stop_playback) {
        printf("[music_player] Playback stopped.\n");
    } else {
        printf("[music_player] Playback completed.\n");
    }

    ret = 0;

    pcm_close(pcm);
    free(buffer);
    fclose(fp);

    return ret;
}

/* Thread function for audio playback */
static void *playback_thread_func(void *arg) {
    char *filename = (char *)arg;
    
    /* Loop the song until stop flag is set */
    while (!g_state.stop_playback && g_state.running) {
        if (play_wav(filename) < 0) {
            /* If playback fails, wait a bit before retrying */
            struct timespec ts = {1, 0};  /* 1 second */
            nanosleep(&ts, NULL);
        }
    }
    
    free(filename);
    return NULL;
}

/* Stop current playback and wait for thread to finish */
static void stop_current_playback(void) {
    pthread_t thread;
    
    pthread_mutex_lock(&g_state.mutex);
    thread = g_state.playback_thread;
    g_state.playback_thread = 0;
    pthread_mutex_unlock(&g_state.mutex);

    if (thread) {
        g_state.stop_playback = 1;
        pthread_join(thread, NULL);
        g_state.stop_playback = 0;
    }
}

/* Start playback of a new WAV file */
static void start_playback(const char *wav_filename) {
    char *filename_copy;
    pthread_t new_thread;

    if (!wav_filename) {
        return;
    }

    /* Stop any current playback */
    stop_current_playback();

    /* Create a copy of filename for the thread */
    filename_copy = strdup(wav_filename);
    if (!filename_copy) {
        fprintf(stderr, "[music_player] Error: Cannot allocate filename copy\n");
        return;
    }

    /* Start new playback thread */
    if (pthread_create(&new_thread, NULL, playback_thread_func, filename_copy) != 0) {
        fprintf(stderr, "[music_player] Error: Cannot create playback thread\n");
        free(filename_copy);
        return;
    }

    pthread_mutex_lock(&g_state.mutex);
    g_state.playback_thread = new_thread;
    pthread_mutex_unlock(&g_state.mutex);
}

/* Signal handler */
static void sig_handler(int sig) {
    (void)sig;
    g_state.running = 0;
}

/* Handle EVT_GIF_CHANGED event */
static void handle_gif_changed(const ipc_event_t *evt) {
    const gif_changed_payload_t *payload = EVT_PAYLOAD(evt, gif_changed_payload_t);
    const char *wav_file;

    printf("[music_player] GIF changed: %s (index %d)\n", 
           payload->gif_name, payload->gif_index);

    /* Look up corresponding WAV file */
    wav_file = find_wav_for_gif(payload->gif_name);
    if (wav_file) {
        printf("[music_player] Playing music: %s\n", wav_file);
        start_playback(wav_file);
    } else {
        printf("[music_player] No music mapping found for: %s\n", payload->gif_name);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    ipc_event_t evt;

    (void)argc;
    (void)argv;

    /* Setup signal handlers */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Connect to event bus */
    printf("[music_player] Connecting to event bus...\n");
    g_state.bus_fd = bus_connect(5);
    if (g_state.bus_fd < 0) {
        fprintf(stderr, "[music_player] Failed to connect to event bus\n");
        return 1;
    }

    /* Subscribe to EVT_GIF_CHANGED */
    uint64_t mask = EVT_MASK(EVT_GIF_CHANGED);
    if (bus_subscribe(g_state.bus_fd, mask) < 0) {
        fprintf(stderr, "[music_player] Failed to subscribe to events\n");
        bus_disconnect(g_state.bus_fd);
        return 1;
    }

    printf("[music_player] Subscribed to EVT_GIF_CHANGED\n");
    printf("[music_player] Waiting for GIF change events...\n");

    /* Main event loop */
    while (g_state.running) {
        if (bus_recv(g_state.bus_fd, &evt) < 0) {
            fprintf(stderr, "[music_player] Event bus connection lost\n");
            break;
        }

        if (evt.type == EVT_GIF_CHANGED) {
            handle_gif_changed(&evt);
        }
    }

    /* Cleanup */
    printf("[music_player] Shutting down...\n");
    stop_current_playback();
    bus_disconnect(g_state.bus_fd);
    
    printf("[music_player] Exiting\n");
    return 0;
}