#ifndef HAVE_TEXT_UTIL_H
#define HAVE_TEXT_UTIL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include "time.h"

#define FONT_FIRST  0x20
#define FONT_LAST   0x5F
#define FONT_W      8
#define FONT_H      16

#define FB_W        240               
#define FB_H        240

/* ── calendar frame renderer ───────────────────────────────────────────────
 *
 * Layout (on a 240×240 display):
 *
 *   y= 70   HH:MM:SS         (scale-2: 16×32, 8 chars → 128 px wide, centred)
 *   y=120   YYYY/MM/DD       (scale-2: 8 chars → 144 px wide, centred)
 *   y=152   DAY OF WEEK      (scale-1, centred)
 *
 * Scale-2 uses a simple 2×2 pixel expansion so the clock is large and
 * readable on a small 240×240 panel without a vector font library.
 */

#define SCALE_TIME  2     /* pixel multiplier for HH:MM:SS */
#define SCALE_DATE  2     /* pixel multiplier for date line */
#define SCALE_DOW   1     /* day-of-week smaller            */

#define GHIBLI_BG_DIR       "/opt/ghibli_bg"

extern const uint8_t g_font[(FONT_LAST - FONT_FIRST + 1)][FONT_H];

void draw_string(uint16_t *buf, int px, int py,
                        const char *s, uint16_t fg, uint16_t bg, int transparent_bg);

void draw_string_centered(uint16_t *buf, int cy,
                                 const char *s, uint16_t fg, int transparent_bg);

void draw_char_scaled(uint16_t *buf, int px, int py,
                              char c, uint16_t fg, int sx, int sy);

void draw_string_scaled_centered(uint16_t *buf, int cy,
                                       const char *s, uint16_t fg, int sx, int sy);

/* Draw text with black outline for readability on any background */
void draw_string_scaled_centered_outlined(uint16_t *buf, int cy,
                                          const char *s, uint16_t fg,
                                          int sx, int sy);

void draw_string_centered_outlined(uint16_t *buf, int cy,
                                   const char *s, uint16_t fg);

/* semi-transparent shadow: darken bg pixels behind text area */
void draw_shadow_band(uint16_t *buf, int y, int h);

void render_calendar_frame(uint16_t *fbmem_hw);

/* calendar framebuffer */
extern uint16_t calendar_buf[FB_W * FB_H];

extern uint16_t g_bg_buf[FB_W * FB_H];    /* cached background */

extern int background_day_idx;   

#endif /* HAVE_TEXT_UTIL_H */