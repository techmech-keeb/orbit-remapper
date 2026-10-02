#pragma once

// The M5Dial screen (M3): LVGL on the GC9A01 panel, driven by the dial and
// the button. Reads only what ble.h and ledger.h offer, through the
// snapshot that the status task hands over once a second; sends commands
// through the orbit_ble_* functions. Nothing here touches the core.

#include <stdbool.h>
#include <stdint.h>
#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool connected;
    int port;
    char name[32];      // ledger display name, or the address when there is no row
    double itvl_ms;
    unsigned latency;
    bool encrypted;
    int subscribed;
    unsigned reports;   // in the last second
    unsigned disconnects;
    uint8_t battery;    // lowest Battery Level in %, or ORBIT_BATTERY_UNKNOWN
} orbit_ui_dev_t;

typedef struct {
    orbit_ui_dev_t dev[ORBIT_MAX_DEVS];
    int devices, max_devices; // ledger rows
    bool pairing;
    int pairing_remaining_s;  // -1: open-ended
    bool full;
    bool duplicates;
    orbit_approval_t approval;
    int low_port, low_level;  // battery-low notice; low_port 0 when none
    const char* usb;          // "none", "suspended", "mounted"
    bool boot_protocol;
    double lat_avg_ms, lat_max_ms; // 0 when nothing was sent
    unsigned heap_free, heap_min;
    char last_event[24];
    const char* version;
} orbit_ui_state_t;

// Takes over the panel that display_init() set up. Frees nothing; the
// caller frees the LVGL reserve first.
bool orbit_ui_init(void);
// Once a second, from the status task.
void orbit_ui_update(const orbit_ui_state_t* s);
// The button was pressed briefly (main loop; never blocks). The screen
// decides what it means: answer an approval question, else open or act on
// the menu.
void orbit_ui_press(void);
// Full-screen notice before a reboot; keeps the screen locked afterwards.
void orbit_ui_message(const char* line1, const char* line2);

#ifdef __cplusplus
}
#endif
