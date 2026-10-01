#pragma once

// BLE central (NimBLE): connects up to ORBIT_MAX_DEVS HID-over-GATT devices
// and hands what the core needs to the main loop through queues. Nothing here
// calls the core (decision I2). Grown from experiments/q31-s3-two-ble.

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORBIT_MAX_DEVS 2
#define ORBIT_REPORT_MAX 64
#define ORBIT_REPORT_MAP_MAX 512

// An input report from a device. interface = slot << 8, as upstream's
// Bluetooth build. report_id is 0 when the device's reports carry no IDs.
typedef struct {
    int64_t t_us;       // esp_timer_get_time() when the notification arrived
    uint16_t interface;
    uint8_t report_id;
    uint8_t len;
    uint8_t data[ORBIT_REPORT_MAX];
} orbit_report_t;

typedef struct {
    uint16_t interface;
    uint8_t hub_port;   // the device's ledger port (1..15); 0 when it has no row
    uint16_t len;
    uint8_t data[ORBIT_REPORT_MAP_MAX];
} orbit_report_map_t;

typedef struct {
    uint8_t slot;
} orbit_disconnect_t;

// Gaps between two reports of the same device, counted per report window.
enum { GAP_LE_8MS, GAP_LE_16MS, GAP_LE_32MS, GAP_OVER_32MS, GAP_BUCKETS };

typedef struct {
    bool connected;
    uint16_t conn_handle;
    uint8_t addr_lo[2]; // lowest two address bytes, [0] = least significant
    int port;           // ledger port (0: none)
    bool encrypted;
    int subscribed;     // input Report characteristics with notifications on
    uint32_t reports;   // reports in this window
    uint32_t max_gap_us;
    uint32_t gaps[GAP_BUCKETS];
    uint32_t total_reports;
    uint32_t disconnects;
} orbit_dev_stats_t;

// wake is notified (xTaskNotifyGive) whenever something is queued.
void orbit_ble_start(TaskHandle_t wake);

// Main loop side. Non-blocking; return false when empty.
bool orbit_ble_take_report(orbit_report_t* out);
bool orbit_ble_take_report_map(orbit_report_map_t* out);
bool orbit_ble_take_disconnect(orbit_disconnect_t* out);

// Reports dropped because the report queue was full (cleared on read).
uint32_t orbit_ble_take_lost(void);

// Call about once a second: restarts scanning when a slot is free, and
// subscribes devices that are still unencrypted after a while.
void orbit_ble_poll(void);

// Config-tool commands (called from the core through platform.h; see platform.cc).
void orbit_ble_pair_new_device(void);
void orbit_ble_clear_bonds(void);
// Back to bonded devices only, without waiting for a pairing (button, G-5).
// Stays in pairing mode when nothing is bonded, as there is nothing to wait for.
void orbit_ble_stop_pairing(void);
// Ledger management (M2 2a). Run on the host task; safe from any task.
void orbit_ble_forget(int port);              // drop the device's bond, row and link
void orbit_ble_move(int new_port, int old_port); // the device on new_port takes over old_port
bool orbit_ble_bonds_full(void);              // a pairing was refused because all 15 ports are taken
bool orbit_ble_duplicates(void);              // two or more ledger rows look like one device; the user picks
int orbit_ble_slot_port(int slot);            // ledger port of the device connected on slot, 0 when none

// Requirement G: a bonded device asking to pair again outside pairing mode.
typedef struct {
    bool wanted;         // a question is open (G-1)
    int port;            // which device
    const char* reason;
    int remaining_s;     // seconds left to answer
    int granted_port;    // 0, or the port that may pair again now (G-2)
    int granted_remaining_s;
} orbit_approval_t;
void orbit_ble_approve(void); // answer the open question: let that device pair again
void orbit_ble_approval(orbit_approval_t* out);
int orbit_ble_pairing_remaining_s(void);      // seconds left in pairing mode; 0 when not pairing, -1 when open-ended

// Copies device slot i and starts a new report window for it.
void orbit_ble_take_stats(int i, orbit_dev_stats_t* out);

int orbit_ble_connected_count(void);
bool orbit_ble_scanning(void);
bool orbit_ble_waiting(void); // the controller is waiting for a bonded device (accept list)
bool orbit_ble_pairing(void); // accepting devices that are not bonded yet

// Short description of the latest connection event, for the screen.
void orbit_ble_last_event(char* buf, int len);

#ifdef __cplusplus
}
#endif
