#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/rtc.h>

#define NTP_SERVER "129.6.15.28" // time.nist.gov
#define NTP_PORT 123
#define NTP_DELTA 2208988800UL

time_t get_ntp_time() {
    int sock;
    struct sockaddr_in server_addr;
    unsigned char packet[48] = {0};

    packet[0] = 0x1B;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NTP_PORT);
    inet_pton(AF_INET, NTP_SERVER, &server_addr.sin_addr);

    sendto(sock, packet, sizeof(packet), 0,
           (struct sockaddr*)&server_addr, sizeof(server_addr));

    recv(sock, packet, sizeof(packet), 0);
    close(sock);

    uint32_t seconds;
    memcpy(&seconds, &packet[40], 4);
    seconds = ntohl(seconds);

    return (time_t)(seconds - NTP_DELTA);
}

void set_system_time(time_t t) {
    struct timeval tv;
    tv.tv_sec = t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

void sync_rtc(time_t t) {
    int fd = open("/dev/rtc0", O_RDWR);
    if (fd < 0) {
        perror("open rtc");
        return;
    }

    struct tm *tm = gmtime(&t);
    struct rtc_time rtc_tm;

    rtc_tm.tm_sec = tm->tm_sec;
    rtc_tm.tm_min = tm->tm_min;
    rtc_tm.tm_hour = tm->tm_hour;
    rtc_tm.tm_mday = tm->tm_mday;
    rtc_tm.tm_mon = tm->tm_mon;
    rtc_tm.tm_year = tm->tm_year;

    if (ioctl(fd, RTC_SET_TIME, &rtc_tm) < 0) {
        perror("RTC_SET_TIME");
    }

    close(fd);
}

int main() {
    time_t t = get_ntp_time();
    printf("NTP time: %s", ctime(&t));

    set_system_time(t);
    printf("System time updated\n");

    sync_rtc(t);
    printf("RTC synced\n");

    return 0;
}