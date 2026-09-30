#pragma once

// Device ledger (M2 stage 2a, decision I4): one row per bonded device, keyed
// by its identity address and type, holding the port number the core sees
// (1..15, never reassigned by itself), what the device told us about itself
// and the user's alias. Kept in NVS next to the config; NimBLE's bond store
// is separate and the two are reconciled at boot.
//
// Safe to call from any task (a mutex guards the table). Nothing here talks
// to NimBLE or the core.

#include <stdbool.h>
#include <stdint.h>
#include "nimble/ble.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORBIT_LEDGER_MAX 15 // upstream's port numbers are 4 bits, 0 = every device
#define ORBIT_LEDGER_TEXT_MAX 24 // name, manufacturer, model and alias, without the terminator

// kind: what the report map describes (bits, several may be set)
#define ORBIT_KIND_KEYBOARD 0x01
#define ORBIT_KIND_MOUSE    0x02
#define ORBIT_KIND_GAMEPAD  0x04
#define ORBIT_KIND_CONSUMER 0x08

// flags
#define ORBIT_LEDGER_NO_KEY 0x01 // row exists but NimBLE holds no bond for it
#define ORBIT_LEDGER_NEW    0x02 // added in this session and not yet checked against older rows (not persisted)

typedef struct {
    uint8_t port; // 0: empty row
    uint8_t flags;
    uint8_t kind;
    ble_addr_t addr;
    uint16_t vid, pid; // from the PnP ID; 0 when the device has none
    uint16_t appearance;
    uint32_t map_hash; // FNV-1a of the report map; 0 until read
    uint32_t last_used; // boot number of the last connection (there is no clock)
    char name[ORBIT_LEDGER_TEXT_MAX + 1];
    char manufacturer[ORBIT_LEDGER_TEXT_MAX + 1];
    char model[ORBIT_LEDGER_TEXT_MAX + 1];
    char alias[ORBIT_LEDGER_TEXT_MAX + 1];
} orbit_ledger_row_t;

void orbit_ledger_init(void);

// Port of addr, or 0 when unknown.
int orbit_ledger_port(const ble_addr_t* addr);
// Adds addr with the lowest free port; returns it, or 0 when the ledger is full.
int orbit_ledger_add(const ble_addr_t* addr, bool is_new);
int orbit_ledger_count(void);
bool orbit_ledger_full(void);
// Copies the row of port; false when there is none.
bool orbit_ledger_get(int port, orbit_ledger_row_t* out);
// Copies the i-th occupied row (0-based, in port order); false past the end.
bool orbit_ledger_nth(int i, orbit_ledger_row_t* out);

// Reconciles with NimBLE's bond list at boot: bonded addresses missing from
// the ledger are added in bond order (which keeps M1's numbering), rows
// without a bond get ORBIT_LEDGER_NO_KEY.
void orbit_ledger_sync_bonds(const ble_addr_t* bonded, int n);

void orbit_ledger_touch(int port);       // connected now
void orbit_ledger_set_key(int port, bool has_key);
void orbit_ledger_set_text(int port, const char* name, const char* manufacturer, const char* model);
void orbit_ledger_set_ids(int port, uint16_t vid, uint16_t pid, uint16_t appearance);
void orbit_ledger_set_map(int port, uint32_t hash, uint8_t kind);
bool orbit_ledger_set_alias(int port, const char* alias);
void orbit_ledger_clear_flag(int port, uint8_t flag);

// Removes the row of port. The caller deletes the bond.
bool orbit_ledger_forget(int port);
// The row of new_port takes over old_port's number (and alias, when it has
// none); the old row is removed. The caller deletes the old bond.
bool orbit_ledger_move(int new_port, int old_port);
// Ports of rows that look like the same device as port (same VID/PID and
// report map hash; without a PnP ID, same name, manufacturer, model and
// hash), excluding port itself. Returns how many were written to out.
int orbit_ledger_similar(int port, int* out, int max);

// What the device is called on the screen and in the log: alias, else name,
// else manufacturer and model, else kind and the low address bytes. When
// another row would read the same, " #<port>" is appended.
void orbit_ledger_display_name(const orbit_ledger_row_t* row, char* out, int len);
// Kind bits of a report map.
uint8_t orbit_ledger_kind_of_map(const uint8_t* map, int len);
const char* orbit_ledger_kind_name(uint8_t kind);

#ifdef __cplusplus
}
#endif
