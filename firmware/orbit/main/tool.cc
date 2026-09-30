// Orbit config-tool commands. See tool.h.
//
// Request (32 bytes, as upstream's set_feature_t):
//   version(1) command(1) data(26) crc32(4)   crc32 over the first 28 bytes
// Reply (32 bytes, as upstream's get_feature_t):
//   data(28) crc32(4)                         crc32 over the first 28 bytes
// Multi-byte numbers are little-endian. Texts are UTF-8 without a
// terminator, blank-padded with zeros. Addresses are the low two bytes only.

#include <cstring>

#include "crc.h"
#include "our_descriptor.h"

#include "ble.h"
#include "ledger.h"
#include "log.h"
#include "orbit.h"
#include "tool.h"

enum : uint8_t {
    ORBIT_GET_STATE = 0x80,   // -> state_reply_t
    ORBIT_GET_ROW = 0x81,     // data: index(1) (0-based, in port order) -> row_reply_t; port 0 past the end
    ORBIT_GET_TEXT = 0x82,    // data: port(1) which(1) -> 24 bytes of text
    ORBIT_SET_ALIAS = 0x83,   // data: port(1) text(24)
    ORBIT_FORGET = 0x84,      // data: port(1)
    ORBIT_MOVE = 0x85,        // data: new_port(1) old_port(1)
    ORBIT_STOP_PAIRING = 0x86,
    ORBIT_APPROVE = 0x87,
};

// which for ORBIT_GET_TEXT
enum : uint8_t { TEXT_NAME, TEXT_MANUFACTURER, TEXT_MODEL, TEXT_ALIAS, TEXT_SHOWN };

#define REPORT_LEN 32
#define PAYLOAD_LEN 28
#define PROTOCOL_VERSION 1

struct __attribute__((packed)) state_reply_t {
    uint8_t protocol;        // PROTOCOL_VERSION
    uint8_t devices;         // ledger rows
    uint8_t max_devices;     // ORBIT_LEDGER_MAX
    uint8_t pairing;         // 0 off, 255 open-ended, else seconds left
    uint8_t full;            // a pairing was refused because the ledger is full
    uint8_t ask_port;        // 0, or the port waiting for approval
    uint8_t ask_remaining_s;
    uint8_t granted_port;    // 0, or the port allowed to pair again now
    uint8_t granted_remaining_s;
    uint8_t duplicates;      // several rows look like one device
    uint8_t connected[ORBIT_MAX_DEVS]; // port per slot, 0 when empty
};

struct __attribute__((packed)) row_reply_t {
    uint8_t port;
    uint8_t flags;
    uint8_t kind;
    uint8_t addr_type;
    uint8_t addr_lo[2];
    uint16_t vid, pid;
    uint32_t map_hash;
    uint16_t appearance;
    uint32_t last_used;
    uint8_t connected;
};

static uint8_t last_command;
static uint8_t last_data[26];

static bool checksum_ok(const uint8_t* buffer) {
    uint32_t crc;
    memcpy(&crc, buffer + PAYLOAD_LEN, 4);
    return crc32(buffer, PAYLOAD_LEN) == crc;
}

static void text_arg(const uint8_t* data, int len, char* out, int out_len) {
    int n = 0;
    for (; n < len && n + 1 < out_len && data[n] != 0; n++) {
        out[n] = (char) data[n];
    }
    out[n] = '\0';
}

bool orbit_tool_set_report(const uint8_t* buffer, uint16_t len) {
    if (len < REPORT_LEN || buffer[1] < 0x80) {
        return false; // upstream's
    }
    if (!checksum_ok(buffer)) {
        olog("M1 EVT t=%.3f tool: command 0x%02x with a bad checksum, ignored\n", orbit_now_s(), buffer[1]);
        return true;
    }
    const uint8_t* data = buffer + 2;
    last_command = buffer[1];
    memcpy(last_data, data, sizeof(last_data));
    switch (buffer[1]) {
    case ORBIT_GET_STATE:
    case ORBIT_GET_ROW:
    case ORBIT_GET_TEXT:
        break; // answered by the next GET_REPORT
    case ORBIT_SET_ALIAS: {
        char text[ORBIT_LEDGER_TEXT_MAX + 1];
        text_arg(data + 1, ORBIT_LEDGER_TEXT_MAX, text, sizeof(text));
        bool ok = orbit_ledger_set_alias(data[0], text);
        olog("M1 EVT t=%.3f tool: alias port %u \"%s\": %s\n", orbit_now_s(), data[0], text, ok ? "set" : "no such port");
        break;
    }
    case ORBIT_FORGET:
        olog("M1 EVT t=%.3f tool: forget port %u\n", orbit_now_s(), data[0]);
        orbit_ble_forget(data[0]);
        break;
    case ORBIT_MOVE:
        olog("M1 EVT t=%.3f tool: move port %u -> %u\n", orbit_now_s(), data[0], data[1]);
        orbit_ble_move(data[0], data[1]);
        break;
    case ORBIT_STOP_PAIRING:
        olog("M1 EVT t=%.3f tool: stop pairing\n", orbit_now_s());
        orbit_ble_stop_pairing();
        break;
    case ORBIT_APPROVE:
        olog("M1 EVT t=%.3f tool: approve\n", orbit_now_s());
        orbit_ble_approve();
        break;
    default:
        olog("M1 EVT t=%.3f tool: unknown command 0x%02x\n", orbit_now_s(), buffer[1]);
        last_command = 0;
        break;
    }
    return true;
}

uint16_t orbit_tool_get_report(uint8_t* buffer, uint16_t reqlen) {
    if (last_command == 0 || reqlen < REPORT_LEN) {
        return 0;
    }
    memset(buffer, 0, REPORT_LEN);
    switch (last_command) {
    case ORBIT_GET_STATE: {
        state_reply_t* r = (state_reply_t*) buffer;
        orbit_approval_t ap;
        orbit_ble_approval(&ap);
        int pairing = orbit_ble_pairing_remaining_s();
        r->protocol = PROTOCOL_VERSION;
        r->devices = orbit_ledger_count();
        r->max_devices = ORBIT_LEDGER_MAX;
        r->pairing = !orbit_ble_pairing() ? 0 : pairing < 0 ? 255 : pairing > 254 ? 254 : pairing;
        r->full = orbit_ble_bonds_full();
        r->ask_port = ap.port;
        r->ask_remaining_s = ap.remaining_s > 255 ? 255 : ap.remaining_s;
        r->granted_port = ap.granted_port;
        r->granted_remaining_s = ap.granted_remaining_s > 255 ? 255 : ap.granted_remaining_s;
        r->duplicates = orbit_ble_duplicates();
        for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
            r->connected[i] = orbit_ble_slot_port(i);
        }
        break;
    }
    case ORBIT_GET_ROW: {
        orbit_ledger_row_t row;
        if (orbit_ledger_nth(last_data[0], &row)) {
            row_reply_t* r = (row_reply_t*) buffer;
            r->port = row.port;
            r->flags = row.flags & ~ORBIT_LEDGER_NEW;
            r->kind = row.kind;
            r->addr_type = row.addr.type;
            r->addr_lo[0] = row.addr.val[0];
            r->addr_lo[1] = row.addr.val[1];
            r->vid = row.vid;
            r->pid = row.pid;
            r->map_hash = row.map_hash;
            r->appearance = row.appearance;
            r->last_used = row.last_used;
            for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
                r->connected |= orbit_ble_slot_port(i) == row.port;
            }
        }
        break;
    }
    case ORBIT_GET_TEXT: {
        orbit_ledger_row_t row;
        if (orbit_ledger_get(last_data[0], &row)) {
            char shown[64];
            const char* text = last_data[1] == TEXT_NAME           ? row.name
                               : last_data[1] == TEXT_MANUFACTURER ? row.manufacturer
                               : last_data[1] == TEXT_MODEL        ? row.model
                               : last_data[1] == TEXT_ALIAS        ? row.alias
                                                                   : shown;
            if (text == shown) {
                orbit_ledger_display_name(&row, shown, sizeof(shown));
            }
            size_t n = strlen(text);
            memcpy(buffer, text, n < PAYLOAD_LEN ? n : PAYLOAD_LEN); // truncation intended: 28-byte payload
        }
        break;
    }
    default:
        return 0;
    }
    uint32_t crc = crc32(buffer, PAYLOAD_LEN);
    memcpy(buffer + PAYLOAD_LEN, &crc, 4);
    last_command = 0;
    return REPORT_LEN;
}
