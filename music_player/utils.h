#ifndef UTILS_H
#define UTILS_H
#include "music_player.h"

int get_playlist_num(struct gif_music_mapping *playlist);

int get_playlist_index(struct gif_music_mapping *playlist, const char *gif);

char *trim_whitespace(char *str);

void free_playlist(struct gif_music_mapping *playlist);


#endif /* UTILS_H */