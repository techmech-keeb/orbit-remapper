// Types a short text to the PC as the USB keyboard. See typist.h.
//
// One report every STEP_US, press then release, on the main loop that
// owns TinyUSB. The core keeps sending its own keyboard reports in between
// when a device's keys change; a report of ours between two of its is just
// one more full keyboard state, so nothing is left pressed.

#include <atomic>
#include <cstring>

#include "esp_timer.h"
#include "tusb.h"

#include "globals.h"
#include "our_descriptor.h"

#include "log.h"
#include "orbit.h"
#include "typist.h"

// Without the scheme: a colon sits on different keys on the US and the
// Japanese layout, and the browser adds https:// itself.
#define CONFIG_URL "www.remapper.org/config/\n"
#define STEP_US 12000 // between two reports; a key is a press and a release
#define KEYBOARD_REPORT_ID 2 // REPORT_ID_KEYBOARD in our_descriptor.cc

static std::atomic<bool> requested;
static bool active;
static const char* text;
static int pos;
static bool key_down;
static int64_t last_us;

// HID keycode of c on the US and the Japanese layout alike; 0 when the two
// differ or c is not supported.
static uint8_t keycode_of(char c) {
    if (c >= 'a' && c <= 'z') {
        return 0x04 + (c - 'a');
    }
    if (c >= '1' && c <= '9') {
        return 0x1E + (c - '1');
    }
    switch (c) {
    case '0': return 0x27;
    case '\n': return 0x28;
    case ' ': return 0x2C;
    case '-': return 0x2D;
    case '.': return 0x37;
    case '/': return 0x38;
    default: return 0;
    }
}

// key 0 releases everything.
static void send_key(uint8_t key) {
    if (boot_protocol_keyboard) {
        uint8_t report[8] = {0}; // modifiers, reserved, 6 keycodes; no report ID
        report[2] = key;
        tud_hid_n_report(0, 0, report, sizeof(report));
        return;
    }
    // kb_mouse and absolute: modifiers, then one bit per key 0x04..0x73, then a byte of extra keys.
    uint8_t report[16] = {0};
    if (key != 0) {
        int i = key - 0x04;
        report[1 + i / 8] |= 1 << (i % 8);
    }
    tud_hid_n_report(0, KEYBOARD_REPORT_ID, report, sizeof(report));
}

bool orbit_typist_available(void) {
    if (boot_protocol_keyboard) {
        return true;
    }
    return our_descriptor != nullptr && (our_descriptor->idx == 0 || our_descriptor->idx == 1);
}

void orbit_typist_request_config_url(void) {
    requested = true;
}

bool orbit_typist_busy(void) {
    return active;
}

void orbit_typist_poll(void) {
    if (requested.exchange(false)) {
        if (active) {
            olog("M1 EVT t=%.3f typist: busy, request dropped\n", orbit_now_s());
        } else if (!orbit_typist_available()) {
            olog("M1 EVT t=%.3f typist: no keyboard in USB descriptor %u, nothing typed\n", orbit_now_s(),
                 our_descriptor != nullptr ? our_descriptor->idx : 255);
        } else if (!tud_mounted()) {
            olog("M1 EVT t=%.3f typist: USB not mounted, nothing typed\n", orbit_now_s());
        } else {
            text = CONFIG_URL;
            pos = 0;
            key_down = false;
            last_us = 0;
            active = true;
            olog("M1 EVT t=%.3f typist: typing the config page address (%d characters, %s)\n", orbit_now_s(),
                 (int) strlen(text) - 1, boot_protocol_keyboard ? "boot protocol" : "report protocol");
        }
    }
    if (!active) {
        return;
    }
    if (!tud_mounted()) {
        active = false;
        olog("M1 EVT t=%.3f typist: USB went away after %d characters\n", orbit_now_s(), pos);
        return;
    }
    int64_t now = esp_timer_get_time();
    if (now - last_us < STEP_US || !tud_hid_n_ready(0)) {
        return;
    }
    if (key_down) {
        send_key(0);
        key_down = false;
        last_us = now;
        pos++;
        if (text[pos] == '\0') {
            active = false;
            olog("M1 EVT t=%.3f typist: done\n", orbit_now_s());
        }
        return;
    }
    uint8_t key = keycode_of(text[pos]);
    if (key == 0) {
        pos++; // not typable on both layouts: skipped
        if (text[pos] == '\0') {
            active = false;
            olog("M1 EVT t=%.3f typist: done\n", orbit_now_s());
        }
        return;
    }
    send_key(key);
    key_down = true;
    last_us = now;
}
