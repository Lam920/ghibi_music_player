#pragma once

#include <stdint.h>

/* ─── Socket path ─────────────────────────────────────────────────────────── */
#define IPC_BUS_PATH        "/tmp/event_bus.sock"

/* ─── Wire constants ──────────────────────────────────────────────────────── */
#define IPC_MAGIC           0xEB
#define IPC_MAX_PAYLOAD     128
/* MAX concurrent clients*/
#define IPC_MAX_CLIENTS     16

/* ─── Event types ─────────────────────────────────────────────────────────── */
typedef enum {
    /* Internal / control */
    EVT_SUBSCRIBE       = 0x00,   /* sent by client on connect to register mask */

    /* Button daemon → bus */
    EVT_BTN_UP          = 0x01,
    EVT_BTN_DOWN        = 0x02,
    EVT_BTN_HOLD        = 0x03,

    /* gif_player → bus */
    EVT_GIF_CHANGED     = 0x10,   /* payload: gif_changed_payload_t */

    /* music_player → bus */
    EVT_MUSIC_CHANGED   = 0x20,   /* payload: music_changed_payload_t */
    EVT_MUSIC_STOPPED   = 0x21,

    /* clock_display → bus */
    EVT_CLOCK_TICK      = 0x30,   /* payload: clock_tick_payload_t */

    EVT_MAX
} event_type_t;

/* ─── Wire frame ──────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t      magic;                   /* must be IPC_MAGIC              */
    event_type_t type;                    /* which event                    */
    uint32_t     timestamp;               /* unix time, set by publisher    */
    uint8_t      payload_len;             /* bytes used in payload[]        */
    uint8_t      payload[IPC_MAX_PAYLOAD];
} __attribute__((packed)) ipc_event_t;

/* ─── Payload for EVT_SUBSCRIBE ───────────────────────────────────────────── */
/*   Sent once by each client right after connect.                             */
/*   event_mask is a bitmask: bit N = subscribe to event type N.               */
/*   Use SUBSCRIBE_ALL to receive every event.                                 */
#define SUBSCRIBE_ALL       0xFFFFFFFFFFFFFFFFULL

typedef struct {
    uint64_t event_mask;
} subscribe_payload_t;

/* ─── Payload for EVT_GIF_CHANGED ─────────────────────────────────────────── */
typedef struct {
    uint8_t gif_index;        /* 0–11                        */
    char    gif_name[64];     /* e.g. "spirited_away.gif"    */
} gif_changed_payload_t;

/* ─── Payload for EVT_MUSIC_CHANGED ──────────────────────────────────────── */
typedef struct {
    uint8_t gif_index;        /* mirrors the gif that triggered this */
    char    wav_name[64];     /* e.g. "spirited_away.wav"            */
} music_changed_payload_t;

/* ─── Payload for EVT_CLOCK_TICK ─────────────────────────────────────────── */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;          /* 0 = Sunday … 6 = Saturday */
} clock_tick_payload_t;

/* ─── Convenience macros ──────────────────────────────────────────────────── */
#define EVT_MASK(type)      (1ULL << (type))

/* Cast payload bytes to a typed struct (no copy). */
#define EVT_PAYLOAD(evt, T) ((T *)((evt)->payload))
