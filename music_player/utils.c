#include "utils.h"
#include <ctype.h>

int get_playlist_num(struct gif_music_mapping *playlist) {
    int count = 0;
    while (playlist[count].gif != NULL && playlist[count].wav != NULL) {
        count++;
    }
    return count;
}

int get_playlist_index(struct gif_music_mapping *playlist, const char *gif) {
    int index = 0;
    while (playlist[index].gif != NULL && playlist[index].wav != NULL) {
        if (strcmp(playlist[index].gif, gif) == 0) {
            return index;
        }
        index++;
    }
    return -1; // Not found
}

char *trim_whitespace(char *str) {
    char *end;
    
    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    return str;
}


void free_playlist(struct gif_music_mapping *playlist) {
    size_t i;
    
    if (playlist != NULL) {
        /* Free until we hit the NULL terminator (where both gif and wav are NULL) */
        for (i = 0; playlist[i].gif != NULL || playlist[i].wav != NULL; i++) {
            if (playlist[i].gif) 
                free((void *)playlist[i].gif);
            if (playlist[i].wav) 
                free((void *)playlist[i].wav);
        }
        free(playlist);
    }
}