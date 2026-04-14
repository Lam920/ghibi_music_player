#ifndef RTC_H
#define RTC_H
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/rtc.h>
#include <errno.h>

#define RTC_DEV             "/dev/rtc0"
#define RESYNC_INTERVAL_S   300                 /* re-read RTC every 5 minutes          */

void resync_rtc_if_needed(void);

#endif /* RTC_H */