#include "rtc.h"

static time_t g_last_rtc_sync = 0;   /* monotonic seconds of last RTC read */

void resync_rtc_if_needed(void)
{
    struct timespec mono;
    struct rtc_time rt;
    struct tm       tm_val;
    int             fd;
    time_t          rtc_t;

    clock_gettime(CLOCK_MONOTONIC, &mono);

    if (g_last_rtc_sync != 0 &&
        (mono.tv_sec - g_last_rtc_sync) < RESYNC_INTERVAL_S)
        return;

    fd = open(RTC_DEV, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[display_manager] Cannot open %s: %s\n",
                RTC_DEV, strerror(errno));
        g_last_rtc_sync = mono.tv_sec;   /* back-off: don't spam */
        return;
    }

    if (ioctl(fd, RTC_RD_TIME, &rt) < 0) {
        fprintf(stderr, "[display_manager] RTC_RD_TIME failed: %s\n", strerror(errno));
        close(fd);
        g_last_rtc_sync = mono.tv_sec;
        return;
    }
    close(fd);

    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_sec  = rt.tm_sec;
    tm_val.tm_min  = rt.tm_min;
    tm_val.tm_hour = rt.tm_hour;
    tm_val.tm_mday = rt.tm_mday;
    tm_val.tm_mon  = rt.tm_mon;          /* already 0-based from kernel */
    tm_val.tm_year = rt.tm_year;         /* years since 1900            */
    tm_val.tm_isdst = -1;

    rtc_t = mktime(&tm_val);
    if (rtc_t == (time_t)-1) {
        fprintf(stderr, "[display_manager] mktime failed\n");
        g_last_rtc_sync = mono.tv_sec;
        return;
    }

    /* Correct CLOCK_REALTIME if drift > 2 s */
    struct timespec wall;
    clock_gettime(CLOCK_REALTIME, &wall);
    long drift = (long)(rtc_t - wall.tv_sec);
    if (drift < -2 || drift > 2) {
        struct timespec new_wall = { .tv_sec = rtc_t, .tv_nsec = 0 };
        if (clock_settime(CLOCK_REALTIME, &new_wall) == 0)
            printf("[display_manager] Corrected CLOCK_REALTIME drift=%lds\n", drift);
        else
            fprintf(stderr, "[display_manager] clock_settime failed: %s (need CAP_SYS_TIME)\n",
                    strerror(errno));
    }

    g_last_rtc_sync = mono.tv_sec;
    printf("[display_manager] RTC synced: %04d-%02d-%02d %02d:%02d:%02d\n",
           rt.tm_year + 1900, rt.tm_mon + 1, rt.tm_mday,
           rt.tm_hour, rt.tm_min, rt.tm_sec);
}
