/*
 * test_subscriber.c
 * Simple test program that subscribes to button events from the event bus
 *
 * Usage: ./test_subscriber
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "bus_client.h"

int main(void) {
    printf("Test Subscriber starting...\n");
    
    // Connect to event bus (retry up to 5 times)
    int bus_fd = bus_connect(5);
    if (bus_fd < 0) {
        fprintf(stderr, "Failed to connect to event bus\n");
        return 1;
    }
    printf("Connected to event bus (fd=%d)\n", bus_fd);
    
    // Subscribe to button events and GIF change events
    uint64_t mask = EVT_MASK(EVT_BTN_UP) | 
                    EVT_MASK(EVT_BTN_DOWN) | 
                    EVT_MASK(EVT_BTN_HOLD) |
                    EVT_MASK(EVT_GIF_CHANGED);
    
    if (bus_subscribe(bus_fd, mask) < 0) {
        fprintf(stderr, "Failed to subscribe\n");
        bus_disconnect(bus_fd);
        return 1;
    }
    printf("Subscribed to events (mask=0x%lx)\n", mask);
    printf("Waiting for events... (Press Ctrl+C to exit)\n\n");
    
    // Main event loop
    ipc_event_t evt;
    while (1) {
        if (bus_recv(bus_fd, &evt) < 0) {
            fprintf(stderr, "Connection lost or error\n");
            break;
        }
        
        // Convert timestamp to readable format
        time_t ts = evt.timestamp;
        struct tm *tm_info = localtime(&ts);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        
        // Handle different event types
        switch (evt.type) {
            case EVT_BTN_UP:
                printf("[%s] 🔘 Button UP\n", time_str);
                break;
                
            case EVT_BTN_DOWN:
                printf("[%s] 🔘 Button DOWN\n", time_str);
                break;
                
            case EVT_BTN_HOLD:
                printf("[%s] 🔘 Button HOLD\n", time_str);
                break;
                
            case EVT_GIF_CHANGED: {
                gif_changed_payload_t *payload = EVT_PAYLOAD(&evt, gif_changed_payload_t);
                printf("[%s] 🎬 GIF changed to: %s (index=%d)\n", 
                       time_str, payload->gif_name, payload->gif_index);
                break;
            }
                
            default:
                printf("[%s] Unknown event type: 0x%02x\n", time_str, evt.type);
                break;
        }
        fflush(stdout);
    }
    
    bus_disconnect(bus_fd);
    printf("Subscriber exiting\n");
    return 0;
}
