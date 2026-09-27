#pragma once

#include <stdbool.h>
#include <stdint.h>

#define Q31_MAX_DEVS 2

// Gaps between two reports of the same device, counted per report window.
// Gaps of 1 s or more are idle time, not delay, and are not counted.
enum { GAP_LE_8MS, GAP_LE_16MS, GAP_LE_32MS, GAP_OVER_32MS, GAP_BUCKETS };

typedef struct {
    bool connected;
    uint16_t conn_handle;
    uint8_t addr_lo[2]; // lowest two address bytes, [0] = least significant
    bool encrypted;
    int subscribed;     // Report characteristics with notifications enabled
    uint32_t reports;   // reports in this window
    uint32_t max_gap_us;
    uint32_t gaps[GAP_BUCKETS];
    uint32_t total_reports;
    uint32_t disconnects;
} q31_dev_stats_t;

void ble_central_start(bool clear_bonds);

// Copies device slot i and starts a new report window for it.
void ble_central_take_stats(int i, q31_dev_stats_t *out);

bool ble_central_scanning(void);

// Short description of the latest connection event, for the screen.
void ble_central_last_event(char *buf, int len);
