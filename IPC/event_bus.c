#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#include "ipc_protocol.h"
#include "bus_client.h"

/*
 * event_bus.c
 *
 * The broker daemon. Responsibilities:
 *   1. Create and own /tmp/event_bus.sock
 *   2. Accept connections from any daemon
 *   3. On EVT_SUBSCRIBE: record which event types that client wants
 *   4. On any other event: fan out to every subscriber whose mask matches
 *
 * Single-threaded, epoll-driven. No daemon needs to know about any other.
 */

/* ─── Client slot ─────────────────────────────────────────────────────────── */

typedef struct {
    int      fd;           /* -1 = slot is free           */
    uint64_t mask;         /* subscribed event bitmask    */
} client_t;

static client_t clients[IPC_MAX_CLIENTS];
static int      client_count = 0;

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

static client_t *client_find_free(void)
{
    for (int i = 0; i < IPC_MAX_CLIENTS; i++)
        if (clients[i].fd == -1)
            return &clients[i];
    return NULL;
}

static client_t *client_find_by_fd(int fd)
{
    for (int i = 0; i < IPC_MAX_CLIENTS; i++)
        if (clients[i].fd == fd)
            return &clients[i];
    return NULL;
}

static void client_remove(int epoll_fd, int client_fd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);

    client_t *c = client_find_by_fd(client_fd);
    if (c) {
        c->fd   = -1;
        c->mask = 0;
        client_count--;
        printf("[event_bus] client fd=%d disconnected (%d remaining)\n",
               client_fd, client_count);
    }
}

/* Fan out evt to every subscriber whose mask includes evt->type */
static void bus_dispatch(ipc_event_t *evt)
{
    for (int i = 0; i < IPC_MAX_CLIENTS; i++) {
        client_t *c = &clients[i];
        if (c->fd == -1)
            continue;
        if (!(c->mask & EVT_MASK(evt->type)))
            continue;

        ssize_t n = send(c->fd, evt, sizeof(*evt), MSG_NOSIGNAL);
        if (n != sizeof(*evt)) {
            fprintf(stderr, "[event_bus] failed to send to fd=%d: %s\n",
                    c->fd, strerror(errno));
            /* Leave cleanup to the next epoll error event on that fd */
        }
    }
}

/* Handle one fully-received event from a connected client */
static void handle_client_event(int epoll_fd, int client_fd, ipc_event_t *evt)
{
    if (evt->magic != IPC_MAGIC) {
        fprintf(stderr, "[event_bus] bad magic from fd=%d, ignoring\n", client_fd);
        return;
    }

    if (evt->type == EVT_SUBSCRIBE) {
        /* Client is registering its subscription mask */
        if (evt->payload_len < sizeof(subscribe_payload_t)) {
            fprintf(stderr, "[event_bus] short SUBSCRIBE payload from fd=%d\n", client_fd);
            return;
        }
        subscribe_payload_t *sub = EVT_PAYLOAD(evt, subscribe_payload_t);
        client_t *c = client_find_by_fd(client_fd);
        if (c) {
            /* Set client interested event mask, not dispatch
            this is only called when a client subscribes to EVT_SUBSCRIBE */
            c->mask = sub->event_mask;
            printf("[event_bus] fd=%d subscribed mask=0x%016llX\n",
                   client_fd, (unsigned long long)c->mask);
        }
        return;
    }

    /* Any other event: log and fan out */
    printf("[event_bus] dispatching event type=0x%02X from fd=%d\n",
           evt->type, client_fd);
    bus_dispatch(evt);
}

/* ─── Signal handling ─────────────────────────────────────────────────────── */

static volatile int g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ─── Main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    /* Init client table */
    for (int i = 0; i < IPC_MAX_CLIENTS; i++)
        clients[i].fd = -1;

    /* Graceful shutdown on SIGINT / SIGTERM */
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);   /* don't die if a client disconnects mid-send */

    /* Create the listening UDS socket */
    unlink(IPC_BUS_PATH);   /* remove stale socket from previous run */

    int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_BUS_PATH, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, IPC_MAX_CLIENTS) < 0) {
        perror("listen"); return 1;
    }
    printf("[event_bus] listening on %s\n", IPC_BUS_PATH);

    /* epoll setup */
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    /* ── Event loop ─────────────────────────────────────────────────────── */
    struct epoll_event events[IPC_MAX_CLIENTS + 1];

    while (g_running) {
        int n = epoll_wait(epoll_fd, events, IPC_MAX_CLIENTS + 1, 500 /* ms timeout */);

        if (n < 0) {
            if (errno == EINTR) continue;   /* woken by signal */
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            /* ── New connection ── */
            if (fd == listen_fd) {
                int client_fd = accept(listen_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[event_bus] accept");
                    continue;
                }
                client_t *slot = client_find_free();
                if (!slot) {
                    fprintf(stderr, "[event_bus] max clients reached, rejecting fd=%d\n",
                            client_fd);
                    close(client_fd);
                    continue;
                }
                slot->fd   = client_fd;
                slot->mask = 0;           /* no events until EVT_SUBSCRIBE arrives */
                client_count++;

                ev.events  = EPOLLIN | EPOLLERR | EPOLLHUP;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

                printf("[event_bus] new client fd=%d (%d total)\n",
                       client_fd, client_count);
                continue;
            }

            /* ── Error / hangup on client ── */
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                client_remove(epoll_fd, fd);
                continue;
            }

            /* ── Data from client ── */
            if (events[i].events & EPOLLIN) {
                ipc_event_t evt;
                ssize_t r = recv(fd, &evt, sizeof(evt), 0);

                if (r == 0 || (r < 0 && errno != EAGAIN)) {
                    client_remove(epoll_fd, fd);
                    continue;
                }
                if ((size_t)r != sizeof(evt)) {
                    fprintf(stderr, "[event_bus] short read from fd=%d (%zd bytes)\n",
                            fd, r);
                    continue;
                }
                handle_client_event(epoll_fd, fd, &evt);
            }
        }
    }

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    printf("[event_bus] shutting down\n");
    for (int i = 0; i < IPC_MAX_CLIENTS; i++)
        if (clients[i].fd != -1)
            close(clients[i].fd);
    close(listen_fd);
    close(epoll_fd);
    unlink(IPC_BUS_PATH);

    return 0;
}
