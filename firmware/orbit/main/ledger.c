// Device ledger. See ledger.h.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "ledger.h"
#include "esp_timer.h"
#include "log.h"

static double now_s(void) {
    return esp_timer_get_time() / 1e6;
}

#define NS  "orbit"
#define KEY "ledger"     // the rows array as it is (session-only flags masked on load)
#define KEY_VERSION "ledger_v"
#define KEY_BOOTS "boots"
#define LEDGER_VERSION 2

// Written to NVS as one blob, straight from this array: a staging copy
// would cost another 2.3 KB of RAM (102144d report: heap_min 9 KB below M1).
static orbit_ledger_row_t rows[ORBIT_LEDGER_MAX];
static uint32_t boots;
static SemaphoreHandle_t mutex;

static void lock(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
}

static void unlock(void) {
    xSemaphoreGive(mutex);
}

// Writes are deferred: a flash write stalls every task for its duration
// (the cache is off meanwhile), and the ledger changes right when a device
// connects, i.e. when input starts. 747c13d report: one 8-9 ms input delay
// per save. So a change only marks the table dirty; orbit_ledger_flush()
// writes it once the input has paused, or after LEDGER_FLUSH_MAX_US at the
// latest, and the several changes of one connection become one write.
#define LEDGER_FLUSH_IDLE_US (1 * 1000000)
#define LEDGER_FLUSH_MAX_US (10 * 1000000)

static volatile bool dirty;
static int64_t dirty_since_us;
static const char* dirty_why;

// Called with the lock held.
static void mark(const char* why) {
    if (!dirty) {
        dirty_since_us = esp_timer_get_time();
        dirty_why = why;
    }
    dirty = true;
}

void orbit_ledger_flush(bool force, int64_t last_input_us) {
    if (!dirty) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (!force && now - last_input_us < LEDGER_FLUSH_IDLE_US && now - dirty_since_us < LEDGER_FLUSH_MAX_US) {
        return;
    }
    lock();
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_VERSION, LEDGER_VERSION);
        if (err == ESP_OK) {
            err = nvs_set_blob(h, KEY, rows, sizeof(rows));
        }
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    const char* why = dirty_why;
    dirty = false;
    unlock();
    if (err != ESP_OK) {
        olog("M1 EVT t=%.3f ledger save failed err=0x%x\n", now_s(), err);
    } else {
        olog("M1 EVT t=%.3f ledger saved (%u B, %.1f ms, why=%s%s)\n", now_s(), (unsigned) sizeof(rows),
             (esp_timer_get_time() - now) / 1000.0, why, force ? ", forced" : "");
    }
}

static orbit_ledger_row_t* row_of_port(int port) {
    if (port <= 0 || port > ORBIT_LEDGER_MAX) {
        return NULL;
    }
    for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
        if (rows[i].port == port) {
            return &rows[i];
        }
    }
    return NULL;
}

static orbit_ledger_row_t* row_of_addr(const ble_addr_t* addr) {
    for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
        if (rows[i].port != 0 && ble_addr_cmp(&rows[i].addr, addr) == 0) {
            return &rows[i];
        }
    }
    return NULL;
}

static int free_port(void) {
    for (int port = 1; port <= ORBIT_LEDGER_MAX; port++) {
        if (row_of_port(port) == NULL) {
            return port;
        }
    }
    return 0;
}

void orbit_ledger_init(void) {
    mutex = xSemaphoreCreateMutex();
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_get_u32(h, KEY_BOOTS, &boots);
        boots++;
        nvs_set_u32(h, KEY_BOOTS, boots);
        uint8_t version = 0;
        nvs_get_u8(h, KEY_VERSION, &version);
        size_t len = sizeof(rows);
        esp_err_t err = version == LEDGER_VERSION ? nvs_get_blob(h, KEY, rows, &len) : ESP_ERR_NVS_NOT_FOUND;
        nvs_commit(h);
        nvs_close(h);
        if (err == ESP_OK && len == sizeof(rows)) {
            for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
                if (rows[i].port < 1 || rows[i].port > ORBIT_LEDGER_MAX) {
                    memset(&rows[i], 0, sizeof(rows[i]));
                }
                rows[i].flags &= ~ORBIT_LEDGER_NEW;
            }
        } else {
            memset(rows, 0, sizeof(rows));
            if (err != ESP_ERR_NVS_NOT_FOUND) {
                olog("M1 EVT t=%.3f ledger load failed err=0x%x len=%u, starting empty\n", now_s(), err, (unsigned) len);
            } else if (version != 0 && version != LEDGER_VERSION) {
                olog("M1 EVT t=%.3f ledger version %u is not %u, starting empty (rows come back from the bonds)\n",
                     now_s(), version, LEDGER_VERSION);
            }
        }
    }
    olog("M1 EVT t=%.3f ledger loaded boot=%lu rows=%d\n", now_s(), (unsigned long) boots, orbit_ledger_count());
}

int orbit_ledger_port(const ble_addr_t* addr) {
    lock();
    orbit_ledger_row_t* r = row_of_addr(addr);
    int port = r != NULL ? r->port : 0;
    unlock();
    return port;
}

int orbit_ledger_add(const ble_addr_t* addr, bool is_new) {
    lock();
    orbit_ledger_row_t* r = row_of_addr(addr);
    if (r != NULL) {
        int port = r->port;
        unlock();
        return port;
    }
    int port = free_port();
    if (port != 0) {
        for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
            if (rows[i].port == 0) {
                r = &rows[i];
                break;
            }
        }
        memset(r, 0, sizeof(*r));
        r->port = port;
        r->addr = *addr;
        r->last_used = boots;
        r->flags = is_new ? ORBIT_LEDGER_NEW : 0;
        mark("add");
    }
    unlock();
    return port;
}

int orbit_ledger_count(void) {
    int n = 0;
    for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
        n += rows[i].port != 0;
    }
    return n;
}

bool orbit_ledger_full(void) {
    return orbit_ledger_count() >= ORBIT_LEDGER_MAX;
}

bool orbit_ledger_get(int port, orbit_ledger_row_t* out) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        *out = *r;
    }
    unlock();
    return r != NULL;
}

bool orbit_ledger_nth(int i, orbit_ledger_row_t* out) {
    lock();
    bool found = false;
    for (int port = 1; port <= ORBIT_LEDGER_MAX && !found; port++) {
        orbit_ledger_row_t* r = row_of_port(port);
        if (r != NULL && i-- == 0) {
            *out = *r;
            found = true;
        }
    }
    unlock();
    return found;
}

void orbit_ledger_sync_bonds(const ble_addr_t* bonded, int n) {
    lock();
    bool changed = false;
    for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
        if (rows[i].port == 0) {
            continue;
        }
        bool has_key = false;
        for (int j = 0; j < n; j++) {
            has_key |= ble_addr_cmp(&rows[i].addr, &bonded[j]) == 0;
        }
        uint8_t flags = has_key ? (rows[i].flags & ~ORBIT_LEDGER_NO_KEY) : (rows[i].flags | ORBIT_LEDGER_NO_KEY);
        changed |= flags != rows[i].flags;
        rows[i].flags = flags;
    }
    unlock();
    for (int j = 0; j < n; j++) {
        if (orbit_ledger_port(&bonded[j]) == 0) {
            int port = orbit_ledger_add(&bonded[j], false);
            olog("M1 EVT t=%.3f ledger: bond addr=..:%02x:%02x had no row, added as port %d\n", now_s(),
                 bonded[j].val[1], bonded[j].val[0], port);
            changed = false; // add() saved
        }
    }
    if (changed) {
        lock();
        mark("sync_bonds");
        unlock();
    }
}

void orbit_ledger_touch(int port) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL && r->last_used != boots) {
        r->last_used = boots;
        mark("touch");
    }
    unlock();
}

void orbit_ledger_set_key(int port, bool has_key) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        uint8_t flags = has_key ? (r->flags & ~ORBIT_LEDGER_NO_KEY) : (r->flags | ORBIT_LEDGER_NO_KEY);
        if (flags != r->flags) {
            r->flags = flags;
            mark("set_key");
        }
    }
    unlock();
}

// Trims the surrounding blanks (the IST Trackball's manufacturer string
// starts with one) and cuts to the row's field size.
static void set_field(char* dst, const char* src) {
    while (*src == ' ') {
        src++;
    }
    size_t n = strlen(src);
    while (n > 0 && src[n - 1] == ' ') {
        n--;
    }
    if (n > ORBIT_LEDGER_TEXT_MAX) {
        n = ORBIT_LEDGER_TEXT_MAX;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void orbit_ledger_set_text(int port, const char* name, const char* manufacturer, const char* model) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        orbit_ledger_row_t before = *r;
        set_field(r->name, name);
        set_field(r->manufacturer, manufacturer);
        set_field(r->model, model);
        if (memcmp(&before, r, sizeof(*r)) != 0) {
            mark("set_text");
        }
    }
    unlock();
}

void orbit_ledger_set_ids(int port, uint16_t vid, uint16_t pid, uint16_t appearance) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL && (r->vid != vid || r->pid != pid || r->appearance != appearance)) {
        r->vid = vid;
        r->pid = pid;
        r->appearance = appearance;
        mark("set_ids");
    }
    unlock();
}

void orbit_ledger_set_map(int port, uint32_t hash, uint8_t kind) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL && (r->map_hash != hash || r->kind != kind)) {
        r->map_hash = hash;
        r->kind = kind;
        mark("set_map");
    }
    unlock();
}

bool orbit_ledger_set_alias(int port, const char* alias) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        set_field(r->alias, alias);
        mark("set_alias");
    }
    unlock();
    return r != NULL;
}

void orbit_ledger_clear_flag(int port, uint8_t flag) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        r->flags &= ~flag;
    }
    unlock();
}

bool orbit_ledger_forget(int port) {
    lock();
    orbit_ledger_row_t* r = row_of_port(port);
    if (r != NULL) {
        memset(r, 0, sizeof(*r));
        mark("forget");
    }
    unlock();
    orbit_ledger_flush(true, 0); // the bond goes with it; keep the two in step
    return r != NULL;
}

bool orbit_ledger_move(int new_port, int old_port) {
    lock();
    orbit_ledger_row_t* n = row_of_port(new_port);
    orbit_ledger_row_t* o = row_of_port(old_port);
    bool ok = n != NULL && o != NULL && n != o;
    if (ok) {
        if (n->alias[0] == '\0') {
            memcpy(n->alias, o->alias, sizeof(n->alias));
        }
        memset(o, 0, sizeof(*o));
        n->port = old_port;
        n->flags &= ~ORBIT_LEDGER_NEW;
        mark("move");
    }
    unlock();
    orbit_ledger_flush(true, 0);
    return ok;
}

int orbit_ledger_similar(int port, int* out, int max) {
    lock();
    int n = 0;
    orbit_ledger_row_t* me = row_of_port(port);
    if (me != NULL && me->map_hash != 0) {
        for (int i = 0; i < ORBIT_LEDGER_MAX && n < max; i++) {
            orbit_ledger_row_t* r = &rows[i];
            if (r->port == 0 || r == me || r->map_hash != me->map_hash) {
                continue;
            }
            bool same;
            if (me->vid != 0 || me->pid != 0) {
                same = r->vid == me->vid && r->pid == me->pid;
            } else {
                same = r->vid == 0 && r->pid == 0 && strcmp(r->name, me->name) == 0 &&
                       strcmp(r->manufacturer, me->manufacturer) == 0 && strcmp(r->model, me->model) == 0;
            }
            if (same) {
                out[n++] = r->port;
            }
        }
    }
    unlock();
    return n;
}

const char* orbit_ledger_kind_name(uint8_t kind) {
    if (kind & ORBIT_KIND_KEYBOARD) {
        return (kind & ORBIT_KIND_MOUSE) ? "keyboard+mouse" : "keyboard";
    }
    if (kind & ORBIT_KIND_MOUSE) {
        return "mouse";
    }
    if (kind & ORBIT_KIND_GAMEPAD) {
        return "gamepad";
    }
    if (kind & ORBIT_KIND_CONSUMER) {
        return "media";
    }
    return "device";
}

// Called with the lock held.
static void base_name(const orbit_ledger_row_t* r, char* out, int len) {
    if (r->alias[0] != '\0') {
        snprintf(out, len, "%s", r->alias);
    } else if (r->name[0] != '\0') {
        snprintf(out, len, "%s", r->name);
    } else if (r->manufacturer[0] != '\0' || r->model[0] != '\0') {
        snprintf(out, len, "%s %s", r->manufacturer, r->model);
    } else {
        snprintf(out, len, "%s ..:%02x:%02x", orbit_ledger_kind_name(r->kind), r->addr.val[1], r->addr.val[0]);
    }
}

void orbit_ledger_display_name(const orbit_ledger_row_t* row, char* out, int len) {
    lock();
    base_name(row, out, len);
    bool clash = false;
    for (int i = 0; i < ORBIT_LEDGER_MAX && !clash; i++) {
        if (rows[i].port == 0 || rows[i].port == row->port) {
            continue;
        }
        char other[ORBIT_LEDGER_TEXT_MAX * 2 + 8];
        base_name(&rows[i], other, sizeof(other));
        clash = strcmp(other, out) == 0;
    }
    unlock();
    if (clash) {
        size_t n = strlen(out);
        snprintf(out + n, len - n, " #%d", row->port);
    }
}

// Walks the report map's short items and notes the Generic Desktop and
// Consumer usages that name the device type (HID 1.11 §6.2.2).
uint8_t orbit_ledger_kind_of_map(const uint8_t* map, int len) {
    uint8_t kind = 0;
    uint32_t page = 0;
    int i = 0;
    while (i < len) {
        uint8_t prefix = map[i++];
        if (prefix == 0xFE) { // long item: bDataSize follows
            if (i >= len) {
                break;
            }
            i += 2 + map[i];
            continue;
        }
        int size = prefix & 3;
        if (size == 3) {
            size = 4;
        }
        if (i + size > len) {
            break;
        }
        uint32_t value = 0;
        for (int k = 0; k < size; k++) {
            value |= (uint32_t) map[i + k] << (8 * k);
        }
        i += size;
        uint8_t tag = prefix & 0xFC;
        if (tag == 0x04) { // Global: Usage Page
            page = value;
        } else if (tag == 0x08) { // Local: Usage
            uint32_t usage_page = size == 4 ? value >> 16 : page;
            uint32_t usage = size == 4 ? (value & 0xFFFF) : value;
            if (usage_page == 0x01) {
                if (usage == 0x06 || usage == 0x07) {
                    kind |= ORBIT_KIND_KEYBOARD;
                } else if (usage == 0x02) {
                    kind |= ORBIT_KIND_MOUSE;
                } else if (usage == 0x04 || usage == 0x05 || usage == 0x08) {
                    kind |= ORBIT_KIND_GAMEPAD;
                }
            } else if (usage_page == 0x0C && usage == 0x01) {
                kind |= ORBIT_KIND_CONSUMER;
            }
        }
    }
    return kind;
}
