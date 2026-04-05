#include "bus_client.h"
#include "ipc_protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

/* ─── Internal helpers ────────────────────────────────────────────────────── */

static int make_uds_fd(void)
{
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
        perror("[bus_client] socket");
    return fd;
}

static void ms_sleep(int ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

int bus_connect(int retry_count)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_BUS_PATH, sizeof(addr.sun_path) - 1);

    for (int attempt = 0; attempt <= retry_count; attempt++) {
        int fd = make_uds_fd();
        if (fd < 0)
            return -1;

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            printf("[bus_client] connected to %s\n", IPC_BUS_PATH);
            return fd;
        }

        /* connect failed — log only on last attempt to avoid spam */
        if (attempt == retry_count) {
            fprintf(stderr, "[bus_client] connect failed after %d attempts: %s\n",
                    retry_count + 1, strerror(errno));
        }

        close(fd);
        ms_sleep(200);
    }

    return -1;
}

int bus_subscribe(int bus_fd, uint64_t event_mask)
{
    ipc_event_t evt;
    bus_evt_init(&evt, EVT_SUBSCRIBE);

    subscribe_payload_t sub = { .event_mask = event_mask };
    memcpy(evt.payload, &sub, sizeof(sub));
    evt.payload_len = sizeof(sub);

    return bus_publish(bus_fd, &evt);
}

int bus_publish(int bus_fd, ipc_event_t *evt)
{
    /* Always stamp magic + timestamp here so callers can't forget */
    evt->magic     = IPC_MAGIC;
    evt->timestamp = (uint32_t)time(NULL);

    ssize_t n = send(bus_fd, evt, sizeof(*evt), MSG_NOSIGNAL);
    if (n != sizeof(*evt)) {
        perror("[bus_client] send");
        return -1;
    }
    return 0;
}

int bus_recv(int bus_fd, ipc_event_t *evt)
{
    ssize_t n = recv(bus_fd, evt, sizeof(*evt), 0);

    if (n == 0) {
        fprintf(stderr, "[bus_client] bus closed connection\n");
        return -1;
    }
    if (n < 0) {
        perror("[bus_client] recv");
        return -1;
    }
    if ((size_t)n != sizeof(*evt)) {
        fprintf(stderr, "[bus_client] short read: got %zd expected %zu\n",
                n, sizeof(*evt));
        return -1;
    }
    if (evt->magic != IPC_MAGIC) {
        fprintf(stderr, "[bus_client] bad magic 0x%02X\n", evt->magic);
        return -1;
    }

    return 0;
}

void bus_evt_init(ipc_event_t *evt, event_type_t type)
{
    memset(evt, 0, sizeof(*evt));
    evt->type = type;
    /* magic + timestamp filled in by bus_publish */
}

void bus_disconnect(int bus_fd)
{
    if (bus_fd >= 0)
        close(bus_fd);
}
