/*
 * test_publisher.c
 * Simple test program that publishes events to the event bus
 *
 * Usage: ./test_publisher [event_type]
 *   event_type: up, down, hold, gif (default: down)
 */

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "bus_client.h"

void print_usage(const char *prog) {
    printf("Usage: %s [event_type]\n", prog);
    printf("  event_type:\n");
    printf("    up      - Send button UP event\n");
    printf("    down    - Send button DOWN event (default)\n");
    printf("    hold    - Send button HOLD event\n");
    printf("    gif     - Send GIF changed event\n");
    printf("    volup   - Send VOLUME UP event\n");
    printf("    voldown - Send VOLUME DOWN event\n");
    printf("\nExamples:\n");
    printf("  %s down\n", prog);
    printf("  %s gif\n", prog);
    printf("  %s volup\n", prog);
}

int main(int argc, char *argv[]) {
    const char *event_str = "down";
    if (argc > 1) {
        event_str = argv[1];
    }
    
    printf("Test Publisher starting...\n");
    
    // Connect to event bus (retry up to 5 times)
    int bus_fd = bus_connect(5);
    if (bus_fd < 0) {
        fprintf(stderr, "Failed to connect to event bus\n");
        fprintf(stderr, "Make sure event_bus daemon is running!\n");
        return 1;
    }
    printf("Connected to event bus (fd=%d)\n", bus_fd);
    
    // We don't need to subscribe, just publish
    // But we need to send a subscribe message anyway (with empty mask)
    if (bus_subscribe(bus_fd, 0) < 0) {
        fprintf(stderr, "Failed to send subscribe\n");
        bus_disconnect(bus_fd);
        return 1;
    }
    
    // Create event based on command line argument
    ipc_event_t evt;
    
    if (strcmp(event_str, "up") == 0) {
        bus_evt_init(&evt, EVT_BTN_UP);
        printf("Publishing: Button UP event\n");
        
    } else if (strcmp(event_str, "down") == 0) {
        bus_evt_init(&evt, EVT_BTN_DOWN);
        printf("Publishing: Button DOWN event\n");
        
    } else if (strcmp(event_str, "hold") == 0) {
        bus_evt_init(&evt, EVT_BTN_HOLD);
        printf("Publishing: Button HOLD event\n");
        
    } else if (strcmp(event_str, "gif") == 0) {
        bus_evt_init(&evt, EVT_GIF_CHANGED);
        gif_changed_payload_t *payload = EVT_PAYLOAD(&evt, gif_changed_payload_t);
        payload->gif_index = 3;
        snprintf(payload->gif_name, sizeof(payload->gif_name), "spirited_away.gif");
        evt.payload_len = sizeof(gif_changed_payload_t);
        printf("Publishing: GIF changed event (gif=%s, index=%d)\n", 
               payload->gif_name, payload->gif_index);
        
    } else if (strcmp(event_str, "volup") == 0) {
        bus_evt_init(&evt, EVT_VOLUME_UP);
        printf("Publishing: Volume UP event\n");
        
    } else if (strcmp(event_str, "voldown") == 0) {
        bus_evt_init(&evt, EVT_VOLUME_DOWN);
        printf("Publishing: Volume DOWN event\n");
        
    } else {
        fprintf(stderr, "Unknown event type: %s\n", event_str);
        print_usage(argv[0]);
        bus_disconnect(bus_fd);
        return 1;
    }
    
    // Publish the event
    if (bus_publish(bus_fd, &evt) < 0) {
        fprintf(stderr, "Failed to publish event\n");
        bus_disconnect(bus_fd);
        return 1;
    }
    
    printf("Event published successfully!\n");
    
    // Give the bus a moment to deliver
    usleep(100000);  // 100ms
    
    bus_disconnect(bus_fd);
    printf("Publisher exiting\n");
    return 0;
}
