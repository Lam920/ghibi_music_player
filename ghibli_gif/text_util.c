#include "text_util.h"

/* clang-format off */
/* ── small 8×16 bitmap font (ASCII 0x20 – 0x5F) ───────────────────────────
 * Each character is 16 bytes; each byte is one row (MSB = leftmost pixel).
 * Only digits 0-9, colon, slash and uppercase A-Z are needed for
 * "HH:MM:SS  YYYY/MM/DD  DDD" – the rest are filled with zeros so they
 * render as blanks without an out-of-range access.
 * Define characters table in byte array
 */

const uint8_t g_font[(FONT_LAST - FONT_FIRST + 1)][FONT_H] = {
    /* 0x20 space */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x21 !     */ { 0,0,0x18,0x18,0x18,0x18,0x18,0x18,0,0x18,0,0,0,0,0,0 },
    /* 0x22 "     */ { 0,0x66,0x66,0x66,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x23 #     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x24 $     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x25 %     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x26 &     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x27 '     */ { 0,0,0x18,0x18,0x18,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x28 (     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x29 )     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x2A *     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x2B +     */ { 0,0,0,0,0x18,0x18,0x7E,0x18,0x18,0,0,0,0,0,0,0 },
    /* 0x2C ,     */ { 0,0,0,0,0,0,0,0,0,0x18,0x18,0x30,0,0,0,0 },
    /* 0x2D -     */ { 0,0,0,0,0,0,0x7E,0,0,0,0,0,0,0,0,0 },
    /* 0x2E .     */ { 0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0,0 },
    /* 0x2F /     */ { 0,0,0x06,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0,0,0,0 },
    /* 0x30 0     */ { 0,0,0x3C,0x66,0x66,0x6E,0x76,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x31 1     */ { 0,0,0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E,0,0,0,0,0,0 },
    /* 0x32 2     */ { 0,0,0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E,0,0,0,0,0,0 },
    /* 0x33 3     */ { 0,0,0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x34 4     */ { 0,0,0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C,0,0,0,0,0,0 },
    /* 0x35 5     */ { 0,0,0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x36 6     */ { 0,0,0x3C,0x66,0x60,0x7C,0x66,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x37 7     */ { 0,0,0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0,0,0,0,0,0 },
    /* 0x38 8     */ { 0,0,0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x39 9     */ { 0,0,0x3C,0x66,0x66,0x66,0x3E,0x06,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x3A :     */ { 0,0,0,0x18,0x18,0,0,0x18,0x18,0,0,0,0,0,0,0 },
    /* 0x3B ;     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x3C <     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x3D =     */ { 0,0,0,0,0x7E,0,0,0x7E,0,0,0,0,0,0,0,0 },
    /* 0x3E >     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x3F ?     */ { 0,0,0x3C,0x66,0x06,0x0C,0x18,0,0x18,0,0,0,0,0,0,0 },
    /* 0x40 @     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x41 A     */ { 0,0,0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0,0,0,0,0,0 },
    /* 0x42 B     */ { 0,0,0x7C,0x66,0x66,0x7C,0x66,0x66,0x66,0x7C,0,0,0,0,0,0 },
    /* 0x43 C     */ { 0,0,0x3C,0x66,0x60,0x60,0x60,0x60,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x44 D     */ { 0,0,0x78,0x6C,0x66,0x66,0x66,0x66,0x6C,0x78,0,0,0,0,0,0 },
    /* 0x45 E     */ { 0,0,0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x7E,0,0,0,0,0,0 },
    /* 0x46 F     */ { 0,0,0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x60,0,0,0,0,0,0 },
    /* 0x47 G     */ { 0,0,0x3C,0x66,0x60,0x60,0x6E,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x48 H     */ { 0,0,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0,0,0,0,0,0 },
    /* 0x49 I     */ { 0,0,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0,0,0,0,0,0 },
    /* 0x4A J     */ { 0,0,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x4B K     */ { 0,0,0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x66,0,0,0,0,0,0 },
    /* 0x4C L     */ { 0,0,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0,0,0,0,0,0 },
    /* 0x4D M     */ { 0,0,0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x63,0,0,0,0,0,0 },
    /* 0x4E N     */ { 0,0,0x66,0x76,0x7E,0x6E,0x66,0x66,0x66,0x66,0,0,0,0,0,0 },
    /* 0x4F O     */ { 0,0,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x50 P     */ { 0,0,0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0,0,0,0,0,0 },
    /* 0x51 Q     */ { 0,0,0x3C,0x66,0x66,0x66,0x66,0x6E,0x3C,0x06,0,0,0,0,0,0 },
    /* 0x52 R     */ { 0,0,0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x66,0,0,0,0,0,0 },
    /* 0x53 S     */ { 0,0,0x3C,0x66,0x60,0x3C,0x06,0x06,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x54 T     */ { 0,0,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0,0 },
    /* 0x55 U     */ { 0,0,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0,0,0 },
    /* 0x56 V     */ { 0,0,0x66,0x66,0x66,0x66,0x66,0x3C,0x3C,0x18,0,0,0,0,0,0 },
    /* 0x57 W     */ { 0,0,0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x63,0,0,0,0,0,0 },
    /* 0x58 X     */ { 0,0,0x66,0x66,0x3C,0x18,0x18,0x3C,0x66,0x66,0,0,0,0,0,0 },
    /* 0x59 Y     */ { 0,0,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0,0,0,0,0,0 },
    /* 0x5A Z     */ { 0,0,0x7E,0x06,0x0C,0x18,0x30,0x60,0x60,0x7E,0,0,0,0,0,0 },
    /* 0x5B [     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x5C \     */ { 0,0,0x60,0x60,0x30,0x30,0x18,0x18,0x0C,0x0C,0x06,0x06,0,0,0,0 },
    /* 0x5D ]     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x5E ^     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    /* 0x5F _     */ { 0,0,0,0,0,0,0,0,0,0,0,0,0x7E,0,0,0 },
};


static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static const char *DAY_NAMES[] = {
    "SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"
};

uint16_t calendar_buf[FB_W * FB_H];

uint16_t g_bg_buf[FB_W * FB_H];
int background_day_idx = -1;

/* ── framebuffer text drawing ──────────────────────────────────────────────*/

/*
 * draw_char – blit one 8×16 character at pixel (px, py) into buf.
 * fg / bg are RGB565; bg == 0 means transparent (skip bg pixels).
 */
static void draw_char(uint16_t *buf, int px, int py,
                      char c, uint16_t fg, uint16_t bg, int transparent_bg)
{
    int row, col;

    if (c < FONT_FIRST || c > FONT_LAST)
        c = ' ';

    const uint8_t *glyph = g_font[(unsigned char)c - FONT_FIRST];

    for (row = 0; row < FONT_H; row++) {
        int fy = py + row;
        if (fy < 0 || fy >= FB_H) continue;

        for (col = 0; col < FONT_W; col++) {
            int fx = px + col;
            if (fx < 0 || fx >= FB_W) continue;

            int bit = (glyph[row] >> (7 - col)) & 1;
            if (bit) {
                buf[fy * FB_W + fx] = fg;
            } else if (!transparent_bg) {
                buf[fy * FB_W + fx] = bg;
            }
        }
    }
}

/* draw_string – left to right, wraps are caller's problem */
void draw_string(uint16_t *buf, int px, int py,
                        const char *s, uint16_t fg, uint16_t bg, int transparent_bg)
{
    while (*s) {
        draw_char(buf, px, py, *s, fg, bg, transparent_bg);
        px += FONT_W;
        s++;
    }
}

/*
 * draw_char_scaled – like draw_char but each pixel is sx×sy screen pixels.
 */
void draw_char_scaled(uint16_t *buf, int px, int py,
                              char c, uint16_t fg, int sx, int sy)
{
    int row, col;

    if (c < FONT_FIRST || c > FONT_LAST) c = ' ';
    const uint8_t *glyph = g_font[(unsigned char)c - FONT_FIRST];

    for (row = 0; row < FONT_H; row++) {
        for (col = 0; col < FONT_W; col++) {
            if (!((glyph[row] >> (7 - col)) & 1)) continue;
            for (int dy = 0; dy < sy; dy++) {
                int fy = py + row * sy + dy;
                if (fy < 0 || fy >= FB_H) continue;
                for (int dx = 0; dx < sx; dx++) {
                    int fx = px + col * sx + dx;
                    if (fx < 0 || fx >= FB_W) continue;
                    buf[fy * FB_W + fx] = fg;
                }
            }
        }
    }
}


/* Draw text with black outline for readability on any background */
void draw_string_scaled_centered_outlined(uint16_t *buf, int cy,
                                          const char *s, uint16_t fg,
                                          int sx, int sy)
{
    int len = (int)strlen(s);
    int total_w = len * FONT_W * sx;
    int px = (FB_W - total_w) / 2;
    int offset = 1;  /* outline thickness in pixels */
    uint16_t outline_color = 0x0000;  /* black outline */
    
    /* Draw outline in 8 directions */
    const char *str;
    int x_start = px;
    int dx, dy;
    
    for (dy = -offset; dy <= offset; dy++) {
        for (dx = -offset; dx <= offset; dx++) {
            if (dx == 0 && dy == 0) continue;  /* skip center */
            str = s;
            x_start = px;
            while (*str) {
                draw_char_scaled(buf, x_start + dx, cy + dy, *str, outline_color, sx, sy);
                x_start += FONT_W * sx;
                str++;
            }
        }
    }
    
    /* Draw foreground text on top */
    while (*s) {
        draw_char_scaled(buf, px, cy, *s, fg, sx, sy);
        px += FONT_W * sx;
        s++;
    }
}

/* Draw centered text with outline */
void draw_string_centered_outlined(uint16_t *buf, int cy,
                                   const char *s, uint16_t fg)
{
    int len = (int)strlen(s);
    int px  = (FB_W - len * FONT_W) / 2;
    int offset = 1;  /* outline thickness */
    uint16_t outline_color = 0x0000;  /* black outline */
    
    /* Draw outline in 8 directions */
    const char *str;
    int x_start;
    int dx, dy;
    
    for (dy = -offset; dy <= offset; dy++) {
        for (dx = -offset; dx <= offset; dx++) {
            if (dx == 0 && dy == 0) continue;
            str = s;
            x_start = px;
            while (*str) {
                draw_char(buf, x_start + dx, cy + dy, *str, outline_color, 0, 1);
                x_start += FONT_W;
                str++;
            }
        }
    }
    
    /* Draw foreground text */
    draw_string(buf, px, cy, s, fg, 0, 1);
}

/* Load background by day of the week */
static void load_background(int day)
{
    char path[256];
    int  fd;
    ssize_t n;

    if (day == background_day_idx)
        return;   /* already loaded */

    snprintf(path, sizeof(path), "%s/%s.bin", GHIBLI_BG_DIR, DAY_NAMES[day % 7]);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        /* fallback: dark teal – still readable with white text */
        uint16_t fill = rgb565(20, 60, 60);
        for (int i = 0; i < FB_W * FB_H; i++)
            g_bg_buf[i] = fill;
        background_day_idx = day;
        fprintf(stderr, "[display_manager] BG %s not found, using fallback colour\n", path);
        return;
    }

    n = read(fd, g_bg_buf, sizeof(g_bg_buf));
    close(fd);

    if (n != (ssize_t)sizeof(g_bg_buf)) {
        fprintf(stderr, "[display_manager] BG %s truncated (%zd/%zu), using fallback\n",
                path, n, sizeof(g_bg_buf));
        uint16_t fill = rgb565(20, 60, 60);
        for (int i = 0; i < FB_W * FB_H; i++)
            g_bg_buf[i] = fill;
    }

    background_day_idx = day;
    printf("[display_manager] Loaded background: %s\n", path);
}

void render_calendar_frame(uint16_t *fbmem_hw)
{
    struct timespec wall;
    struct tm       now;
    char            time_str[16];   /* HH:MM:SS */
    char            date_str[16];   /* YYYY/MM/DD */

    /* 1. Get current wall-clock time */
    clock_gettime(CLOCK_REALTIME, &wall);
    localtime_r(&wall.tv_sec, &now);

    /* 2. Load today's background (cached by yday) */
    load_background(now.tm_wday);   /* wday is 0-based; files are 0…6 */

    /* 3. Copy bg → off-screen buffer */
    memcpy(calendar_buf, g_bg_buf, sizeof(calendar_buf));

    /* 4. Calculate text positions */
    int time_y  = (FB_H / 2) - (FONT_H * SCALE_TIME) - 8;
    int date_y  = (FB_H / 2) + 4;
    int dow_y   = date_y + FONT_H * SCALE_DATE + 6;

    /* 5. Compose text */
    uint16_t fg_white  = rgb565(255, 255, 255);
    uint16_t fg_yellow = rgb565(255, 240, 120);

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             now.tm_hour, now.tm_min, now.tm_sec);
    snprintf(date_str, sizeof(date_str), "%04d/%02d/%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

    /* Draw with black outline for readability on any background */
    draw_string_scaled_centered_outlined(calendar_buf, time_y, time_str,
                                         fg_white, SCALE_TIME, SCALE_TIME);
    draw_string_scaled_centered_outlined(calendar_buf, date_y, date_str,
                                         fg_yellow, SCALE_DATE, SCALE_DATE);
    draw_string_scaled_centered_outlined(calendar_buf, dow_y,
                                  DAY_NAMES[now.tm_wday], fg_yellow, 1, 1);

#if USE_BGR565
    convert_frame_to_bgr(calendar_buf, FB_W * FB_H);
#endif

    /* 6. Blit to hardware framebuffer */
    memcpy(fbmem_hw, calendar_buf, sizeof(calendar_buf));
}
