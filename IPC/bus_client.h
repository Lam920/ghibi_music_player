#pragma once

#include <stdint.h>
#include "ipc_protocol.h"

/*
 * bus_client.h
 *
 * Thin helper library linked into every daemon.
 * Does NOT create any socket — only connects to the one owned by event_bus.
 *
 * Typical daemon startup:
 *
 *   int bus_fd = bus_connect();
 *   bus_subscribe(bus_fd, EVT_MASK(EVT_BTN_UP) | EVT_MASK(EVT_BTN_DOWN));
 *
 *   // inside epoll loop:
 *   ipc_event_t evt;
 *   if (bus_recv(bus_fd, &evt) == 0)
 *       handle(&evt);
 */

/* Connect to the event bus.
 * Retries up to `retry_count` times with 200 ms delay between attempts.
 * Returns a connected fd on success, -1 on failure. */
int bus_connect(int retry_count);

/* Register which event types this daemon wants to receive.
 * Call once right after bus_connect().
 * event_mask: bitmask built with EVT_MASK(). Use SUBSCRIBE_ALL for everything.
 * Returns 0 on success, -1 on error. */
int bus_subscribe(int bus_fd, uint64_t event_mask);

/* Publish one event onto the bus so all interested subscribers receive it.
 * Fills in magic and timestamp automatically.
 * Returns 0 on success, -1 on error. */
int bus_publish(int bus_fd, ipc_event_t *evt);

/* Blocking receive of one event from the bus.
 * Returns 0 on success, -1 on error / connection closed. */
int bus_recv(int bus_fd, ipc_event_t *evt);

/* Helper: initialise an event struct cleanly before filling payload. */
void bus_evt_init(ipc_event_t *evt, event_type_t type);

/* Close the connection to the bus. */
void bus_disconnect(int bus_fd);
