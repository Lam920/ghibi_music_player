#include "utils.h"

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