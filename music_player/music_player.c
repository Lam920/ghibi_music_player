#include "music_player.h"
#include "../IPC/bus_client.h"
#include "../IPC/ipc_protocol.h"
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include "utils.h"
#include <ctype.h>

/* Dynamic playlist array */
struct gif_music_mapping *playlist = NULL;
static size_t playlist_count = 0;
static size_t playlist_capacity = 0;

static playback_state_t g_state = {
    .stop_playback = 0,
    .running = 1,
    .playback_thread = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .bus_fd = -1,
    .volume = VOLUME_DEFAULT
};

/* Load playlist configuration from file */
static int load_config(const char *config_path) {
    FILE *fp;
    char line[512];
    char *gif_name, *wav_name, *delimiter;
    size_t line_num = 0;
    
    fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "[music_player] Error: Cannot open config file %s: %s\n", 
                config_path, strerror(errno));
        return -1;
    }
    
    printf("[music_player] Loading config from %s\n", config_path);
    
    /* Free any existing playlist */
    free_playlist(playlist);
    playlist = NULL;
    playlist_count = 0;
    
    /* Initial allocation */
    playlist_capacity = 16;
    playlist = calloc(playlist_capacity, sizeof(struct gif_music_mapping));
    if (!playlist) {
        fprintf(stderr, "[music_player] Error: Cannot allocate playlist memory\n");
        fclose(fp);
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        /* Trim whitespace */
        gif_name = trim_whitespace(line);
        
        /* Skip empty lines and comments */
        if (gif_name[0] == '\0' || gif_name[0] == '#') {
            continue;
        }
        
        /* Find delimiter '=' */
        delimiter = strchr(gif_name, '=');
        if (!delimiter) {
            fprintf(stderr, "[music_player] Warning: Invalid format at line %zu (expected gif=wav.wav)\n", 
                    line_num);
            continue;
        }
        
        /* Split into gif_name and wav_name */
        *delimiter = '\0';
        wav_name = delimiter + 1;
        
        /* Trim both parts */
        gif_name = trim_whitespace(gif_name);
        wav_name = trim_whitespace(wav_name);
        
        /* Validate */
        if (gif_name[0] == '\0' || wav_name[0] == '\0') {
            fprintf(stderr, "[music_player] Warning: Empty gif or wav name at line %zu\n", 
                    line_num);
            continue;
        }
        
        /* Expand array if needed */
        if (playlist_count >= playlist_capacity - 1) {
            size_t new_capacity = playlist_capacity * 2;
            struct gif_music_mapping *new_playlist = realloc(playlist, 
                    new_capacity * sizeof(struct gif_music_mapping));
            if (!new_playlist) {
                fprintf(stderr, "[music_player] Error: Cannot expand playlist memory\n");
                free_playlist(playlist);
                playlist = NULL;
                playlist_count = 0;
                fclose(fp);
                return -1;
            }
            playlist = new_playlist;
            playlist_capacity = new_capacity;
            /* Zero out new memory */
            memset(&playlist[playlist_count], 0, 
                   (new_capacity - playlist_count) * sizeof(struct gif_music_mapping));
        }
        
        /* Store the mapping */
        playlist[playlist_count].gif = strdup(gif_name);
        playlist[playlist_count].wav = strdup(wav_name);
        
        if (!playlist[playlist_count].gif || !playlist[playlist_count].wav) {
            fprintf(stderr, "[music_player] Error: Cannot allocate string memory\n");
            free_playlist(playlist);
            playlist = NULL;
            playlist_count = 0;
            fclose(fp);
            return -1;
        }
        
        printf("[music_player]   %s -> %s\n", 
               playlist[playlist_count].gif, 
               playlist[playlist_count].wav);
        
        playlist_count++;
    }
    
    /* Add NULL terminator entry */
    playlist[playlist_count].gif = NULL;
    playlist[playlist_count].wav = NULL;
    
    fclose(fp);
    
    printf("[music_player] Loaded %zu mappings from config\n", playlist_count);
    
    if (playlist_count == 0) {
        fprintf(stderr, "[music_player] Warning: No valid mappings found in config file\n");
    }
    
    return 0;
}

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

/* Apply volume to PCM buffer (in-place) */
static void apply_volume(char *buffer, size_t bytes, int bits_per_sample, int volume) {
    size_t i;
    
    if (volume >= 100) {
        return;  /* No attenuation needed */
    }
    
    if (bits_per_sample == 16) {
        /* 16-bit signed samples */
        int16_t *samples = (int16_t *)buffer;
        size_t num_samples = bytes / sizeof(int16_t);
        for (i = 0; i < num_samples; i++) {
            samples[i] = (int16_t)((samples[i] * volume) / 100);
        }
    } else if (bits_per_sample == 8) {
        /* 8-bit unsigned samples (offset by 128) */
        uint8_t *samples = (uint8_t *)buffer;
        for (i = 0; i < bytes; i++) {
            int16_t val = (int16_t)samples[i] - 128;
            val = (val * volume) / 100;
            samples[i] = (uint8_t)(val + 128);
        }
    }
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

    // Playback loop with stop flag checking and volume control
    while (!g_state.stop_playback && (bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
        int current_volume;
        
        /* Get current volume (thread-safe) */
        pthread_mutex_lock(&g_state.mutex);
        current_volume = g_state.volume;
        pthread_mutex_unlock(&g_state.mutex);
        
        /* Apply volume adjustment */
        apply_volume(buffer, bytes_read, header.bits_per_sample, current_volume);
        
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

/* Handle volume control events */
static void handle_volume_event(event_type_t type) {
    int new_volume;
    
    pthread_mutex_lock(&g_state.mutex);
    
    new_volume = g_state.volume;
    
    if (type == EVT_VOLUME_UP) {
        new_volume += VOLUME_STEP;
        if (new_volume > VOLUME_MAX) {
            new_volume = VOLUME_MAX;
        }
    } else if (type == EVT_VOLUME_DOWN) {
        new_volume -= VOLUME_STEP;
        if (new_volume < VOLUME_MIN) {
            new_volume = VOLUME_MIN;
        }
    }
    
    if (new_volume != g_state.volume) {
        g_state.volume = new_volume;
        pthread_mutex_unlock(&g_state.mutex);
        printf("[music_player] Volume: %d%%\n", new_volume);
    } else {
        pthread_mutex_unlock(&g_state.mutex);
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

    /* Load configuration */
    printf("[music_player] Starting Music Player daemon...\n");
    if (load_config(CONFIG_FILE) < 0) {
        fprintf(stderr, "[music_player] Failed to load configuration from %s\n", CONFIG_FILE);
        return 1;
    }

    /* Connect to event bus */
    printf("[music_player] Connecting to event bus...\n");
    g_state.bus_fd = bus_connect(5);
    if (g_state.bus_fd < 0) {
        fprintf(stderr, "[music_player] Failed to connect to event bus\n");
        return 1;
    }

    /* Subscribe to EVT_GIF_CHANGED and volume events */
    uint64_t mask = EVT_MASK(EVT_GIF_CHANGED) | EVT_MASK(EVT_VOLUME_UP) | EVT_MASK(EVT_VOLUME_DOWN);
    if (bus_subscribe(g_state.bus_fd, mask) < 0) {
        fprintf(stderr, "[music_player] Failed to subscribe to events\n");
        bus_disconnect(g_state.bus_fd);
        return 1;
    }

    printf("[music_player] Subscribed to EVT_GIF_CHANGED, EVT_VOLUME_UP, EVT_VOLUME_DOWN\n");
    printf("[music_player] Initial volume: %d%%\n", g_state.volume);
    printf("[music_player] Waiting for events...\n");

    /* Main event loop */
    while (g_state.running) {
        if (bus_recv(g_state.bus_fd, &evt) < 0) {
            fprintf(stderr, "[music_player] Event bus connection lost\n");
            break;
        }

        switch (evt.type) {
            case EVT_GIF_CHANGED:
                handle_gif_changed(&evt);
                break;
            case EVT_VOLUME_UP:
            case EVT_VOLUME_DOWN:
                handle_volume_event(evt.type);
                break;
            default:
                break;
        }
    }

    /* Cleanup */
    printf("[music_player] Shutting down...\n");
    stop_current_playback();
    bus_disconnect(g_state.bus_fd);
    free_playlist(playlist);
    
    printf("[music_player] Exiting\n");
    return 0;
}