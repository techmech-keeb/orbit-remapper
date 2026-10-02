// BLE central for Orbit M1. See ble.h. Runs on the NimBLE host task (CPU0).
//
// Compared with the Q31 experiment this also reads each Report
// characteristic's Report Reference (ID and type), subscribes to the input
// ones only, reads the Report Map and queues reports, report maps and
// disconnects for the main loop, and scans for bonded devices only unless
// pair_new_device() was called (as upstream's Bluetooth build).

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble.h"
#include "ledger.h"
#include "log.h"

#define UUID_HID_SERVICE 0x1812
#define UUID_REPORT_MAP  0x2A4B
#define UUID_REPORT      0x2A4D
#define UUID_CCCD        0x2902
#define UUID_REPORT_REF  0x2908
// Read once per link after subscribing, for the device ledger (requirement B).
#define UUID_DEVICE_NAME  0x2A00
#define UUID_APPEARANCE   0x2A01
#define UUID_MODEL_NUMBER 0x2A24
#define UUID_MANUFACTURER 0x2A29
#define UUID_PNP_ID       0x2A50

#define UUID_BATTERY_SERVICE 0x180F
#define UUID_BATTERY_LEVEL   0x2A19
#define UUID_USER_DESC       0x2901
#define UUID_PRESENTATION    0x2904

#define REPORT_TYPE_INPUT 1

#define MAX_HID_SVCS 2
#define MAX_CHRS     32
#define MAX_BAT      4 // Battery Level characteristics per device (a split keyboard has one per half)
#define CONNECT_TIMEOUT_MS 10000 // one direct connect, or one round of accept-list waiting
#define RECONNECT_HOLD_MS 500     // pause after a disconnect before the next attempt (RMK, upstream BLE)
// While waiting for bonded devices the controller listens 30 ms out of
// every 60 ms for the first 30 s after a disconnect or start (HOGP 5.2.4),
// then 30 ms out of every 300 ms. HOGP's long-term figure (11.25 ms per
// 1.28 s) is for battery-powered hosts; the M5Dial is USB-powered and a
// woken keyboard should connect within a second.
#define FAST_WAIT_US (30 * 1000000)
// NimBLE cancels a connect attempt after CONNECT_TIMEOUT_MS and then waits
// for the controller's "connection complete" event, which may never come
// (ble_gap.c, ble_gap_master_timer: "XXX: Set a timer to reset the
// controller..."). Seen twice when a device vanished right after
// advertising (09b3075 report): the attempt hung until RST.
#define CONNECT_STUCK_CANCEL_US (15 * 1000000)
#define CONNECT_STUCK_RESET_US  (25 * 1000000)
#define ENC_WAIT_US (5 * 1000000)         // from our encryption request to an answer
#define SEC_PENDING_MAX_US (15 * 1000000) // waiting for NimBLE's one SM slot to free up
// The first GATT request on a link made right after the device dropped the
// previous one sometimes gets no answer until the 30 s ATT timeout
// (a195bc8 report, problem G'). A fresh link answers within 0.5 s.
#define DISCOVERY_STALL_US (5 * 1000000)

// Connection parameters requested in the connection request (q31-results.md
// §5: asking after connecting is refused for the second device).
#define CONN_ITVL   6   // 7.5 ms
#define CONN_LATENCY 0
#define CONN_TIMEOUT 400 // 4 s

#define REPORT_QUEUE_LEN 32

// Pairing mode ends by itself after this (upstream waits forever; a forgotten
// Pair new device would keep accepting anyone). Not while nothing is bonded:
// then there is nothing else to wait for.
#define PAIRING_TIMEOUT_US (120 * 1000000)

// Requirement G: a bonded device that wants to pair again outside pairing
// mode (it lost its key, or brings a new one) is refused, and the user is
// asked. A short press of the button grants that one address a window to
// pair afresh; nothing else can pair meanwhile. A prompt that is not
// answered counts as a refusal; after APPROVAL_MAX_REFUSALS the device is
// not asked about again until restart (G-4).
#define APPROVAL_PROMPT_US  (60 * 1000000)
#define APPROVAL_WINDOW_US  (60 * 1000000)
#define APPROVAL_MAX_REFUSALS 3

// A device whose encryption failed (it still holds keys from an earlier
// pairing) reconnects at once and fails again: 280 attempts in 2 minutes,
// crowding out other devices (09b3075 report). After MAX_FAILS such
// failures in a row it is left out of the accept list for AVOID_US (nRF
// Desktop allows two attempts per device before filtering it). Only
// failures that a reconnect cannot fix count (give_up_on()); a link that
// dies while being set up (0x3E) is retried at once, as before stage 2
// (1fd6c6a report: two 0x3E in a row kept meteorite40 out for 38 s).
#define MAX_FAILS 2
#define AVOID_US (30 * 1000000)
#define AVOID_MAX 4

// Provided by NimBLE's NVS-backed store; it has no public header.
void ble_store_config_init(void);

typedef struct {
    uint16_t def_handle;
    uint16_t val_handle;
    uint16_t end_handle;
    uint16_t uuid16;
    uint8_t props;
    // Report characteristics only
    uint16_t cccd_handle;
    uint16_t ref_handle;
    uint8_t report_id;
    uint8_t report_type;
} chr_t;

typedef struct {
    uint16_t def_handle, val_handle, end_handle;
    uint16_t cccd_handle, user_desc_handle, presentation_handle;
    uint8_t props;
} bat_chr_t;

typedef struct dev dev_t;

// Handed to NimBLE as the callback argument of every GATT procedure. A
// procedure of a link that has since gone (and whose conn_handle the next
// link may reuse) is told apart by the generation.
typedef struct {
    dev_t* d;
    uint32_t gen;
} gatt_ctx_t;

struct dev {
    // Connection state; slot is in use when connecting or connected.
    bool in_use;
    uint32_t gen;      // bumped on every connect and disconnect
    gatt_ctx_t* ctx;   // context of this link's GATT procedures
    bool connected;
    uint16_t conn_handle;
    ble_addr_t addr;
    bool encrypted;
    bool repaired; // already retried pairing after the device lost our bond
    int64_t connected_us;
    bool sec_pending; // ble_gap_security_initiate() failed, retry from periodic_check()
    int64_t sec_started_us; // when our encryption/pairing request went out (0: not yet)

    bool discovering;
    bool discovery_finished;
    int64_t discovery_progress_us; // last GATT callback of this link
    bool rediscover;       // encryption came up while discovering unencrypted
    bool peer_allows_itvl; // latest device request includes our interval
    bool reasserted;       // already asked again for our interval

    // GATT discovery
    uint16_t svc_start[MAX_HID_SVCS];
    uint16_t svc_end[MAX_HID_SVCS];
    int n_svcs;
    int cur_svc;
    chr_t chrs[MAX_CHRS];
    int n_chrs;
    int cur_chr;
    int subscribed;
    uint16_t report_map_handle;
    orbit_report_map_t report_map;
    int info_step; // next entry of info_reads[] to read after subscribing (requirement B)
    // What the device said about itself, handed to the ledger once all reads are done.
    char info_name[ORBIT_LEDGER_TEXT_MAX + 1];
    char info_manufacturer[ORBIT_LEDGER_TEXT_MAX + 1];
    char info_model[ORBIT_LEDGER_TEXT_MAX + 1];
    uint16_t info_vid, info_pid, info_appearance;
    bool map_pending; // report map held back until the ledger row is settled (new device)
    uint32_t map_hash;

    // Battery Service, probed after the device information (stage 0 of the
    // battery display: which devices offer it, how many, and whether they
    // notify). Notifications on these handles are not input reports.
    bool bat_started;
    int bat_phase;
    uint16_t bat_svc_start[MAX_BAT], bat_svc_end[MAX_BAT];
    int bat_n_svcs;
    bat_chr_t bat[MAX_BAT];
    int bat_n;
    int bat_cur; // service while finding characteristics, then characteristic

    // Report statistics
    int64_t last_report_us;
    uint32_t reports;
    uint32_t max_gap_us;
    uint32_t gaps[GAP_BUCKETS];
    uint32_t total_reports;
    uint32_t disconnects;
    uint8_t stalls; // discovery stalls in a row on this slot's device (kept across reconnects)
    uint32_t key_tag; // fingerprint of the stored LTK when the link came up (0: none), never logged
};

// Stale procedures die within the 30 s ATT timeout, so a small ring suffices.
static gatt_ctx_t gatt_ctxs[8];
static unsigned gatt_ctx_next;

static gatt_ctx_t* new_gatt_ctx(dev_t* d) {
    gatt_ctx_t* c = &gatt_ctxs[gatt_ctx_next++ % (sizeof(gatt_ctxs) / sizeof(gatt_ctxs[0]))];
    c->d = d;
    c->gen = d->gen;
    return c;
}

// True when the callback belongs to the link the slot currently holds.
static dev_t* gatt_ctx_dev(void* arg, uint16_t conn_handle) {
    gatt_ctx_t* c = arg;
    dev_t* d = c->d;
    if (!d->connected || d->conn_handle != conn_handle || c->gen != d->gen) {
        return NULL;
    }
    d->discovery_progress_us = esp_timer_get_time();
    return d;
}

static const char* TAG = "ble";
static dev_t devs[ORBIT_MAX_DEVS];
static portMUX_TYPE stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t own_addr_type;
static bool connecting;      // a direct connect to a scanned device is in flight
static bool wl_waiting;      // the controller is waiting for any bonded device (accept list)
static int64_t connecting_since_us; // start of either of the above
static bool connecting_cancel_logged;
static int64_t hold_until_us;       // no new attempt before this (RECONNECT_HOLD_MS after a disconnect)
static int64_t fast_until_us;       // listen at the high duty cycle until then
static int64_t wait_since_us;       // start of the current accept-list wait, over all its rounds (0: none)
static int wait_logged_n = -1;      // device count and duty of the last "waiting for" line
static bool wait_logged_fast;
static struct ble_npl_callout resume_co;
static volatile bool scanning;
static volatile bool peers_only = true; // scan for bonded devices only
static int last_scan_rc;
static struct ble_npl_event periodic_ev, pair_ev, stop_pair_ev, clear_bonds_ev, cmd_ev;
static int64_t pairing_since_us; // start of the current pairing mode (0: not pairing)
static bool bonds_full_seen;     // a pairing was refused because the ledger is full (cleared by forget)

// Ledger commands from other tasks (config tool, serial), run on the host task.
typedef struct {
    enum { CMD_FORGET, CMD_MOVE } kind;
    int a, b;
} cmd_t;
static QueueHandle_t cmd_q;
static struct ble_npl_event approve_ev;

static struct {
    bool wanted;
    ble_addr_t addr;
    int port;
    const char* reason;
    int64_t since_us;
} approval; // the open question (G-1)
static struct {
    ble_addr_t addr;
    int64_t until_us; // 0: none
} approved; // the granted window (G-2)
static struct {
    ble_addr_t addr;
    uint8_t refusals;
} refusals[ORBIT_LEDGER_MAX]; // unanswered prompts per address (G-4)
static char last_event[24] = "START";
static bool duplicates_seen; // two or more ledger rows look like one device (info_done)
static TaskHandle_t wake_task;
static QueueHandle_t report_q, report_map_q, disconnect_q;
static uint32_t reports_lost;

static struct {
    ble_addr_t addr;
    uint8_t fails;    // failures in a row
    int64_t until_us; // excluded until then (0: not excluded)
} trouble[AVOID_MAX];

static int trouble_slot(const ble_addr_t* addr) {
    int64_t now = esp_timer_get_time();
    int slot = 0;
    for (int i = 0; i < AVOID_MAX; i++) {
        if (ble_addr_cmp(&trouble[i].addr, addr) == 0) {
            return i;
        }
        // Otherwise reuse a free or expired entry, else the oldest.
        if (trouble[i].fails == 0 && trouble[i].until_us <= now) {
            slot = i;
        } else if (trouble[i].until_us < trouble[slot].until_us) {
            slot = i;
        }
    }
    trouble[slot].addr = *addr;
    trouble[slot].fails = 0;
    trouble[slot].until_us = 0;
    return slot;
}

// Returns true when the device has just been excluded.
static bool fail_note(const ble_addr_t* addr) {
    int i = trouble_slot(addr);
    if (++trouble[i].fails < MAX_FAILS) {
        return false;
    }
    trouble[i].fails = 0;
    trouble[i].until_us = esp_timer_get_time() + AVOID_US;
    return true;
}

static void fail_clear(const ble_addr_t* addr) {
    for (int i = 0; i < AVOID_MAX; i++) {
        if (ble_addr_cmp(&trouble[i].addr, addr) == 0) {
            trouble[i].fails = 0;
            trouble[i].until_us = 0;
        }
    }
}

static bool avoided(const ble_addr_t* addr) {
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < AVOID_MAX; i++) {
        if (trouble[i].until_us > now && ble_addr_cmp(&trouble[i].addr, addr) == 0) {
            return true;
        }
    }
    return false;
}

static int gap_event(struct ble_gap_event* event, void* arg);
static void discover_next_report(dev_t* d);
static void start_discovery(dev_t* d);
static void schedule(void);
static bool is_bonded(const ble_addr_t* addr);
static int ensure_row(dev_t* d);
static void stop_pairing_ev(struct ble_npl_event* ev);

static double now_s(void) {
    return esp_timer_get_time() / 1e6;
}

static double ms(uint16_t units_1250us) {
    return units_1250us * 1.25;
}

static int hci(int status) {
    return status >= BLE_HS_ERR_HCI_BASE ? status - BLE_HS_ERR_HCI_BASE : -1;
}

static void set_last_event(const char* what, int slot, int status) {
    portENTER_CRITICAL(&stats_mux);
    if (status < 0) {
        snprintf(last_event, sizeof(last_event), "%s D%d", what, slot);
    } else {
        snprintf(last_event, sizeof(last_event), "%s D%d %X", what, slot, status);
    }
    portEXIT_CRITICAL(&stats_mux);
}

// Event lines share one prefix so they can be grepped out of a long log.
#define EVT(d, fmt, ...) \
    olog("M1 EVT t=%.3f D%d addr=..:%02x:%02x " fmt "\n", now_s(), (int) ((d) - devs), (d)->addr.val[1], \
         (d)->addr.val[0], ##__VA_ARGS__)

static bool approved_for(const ble_addr_t* addr) {
    return approved.until_us > esp_timer_get_time() && ble_addr_cmp(&approved.addr, addr) == 0;
}

static uint8_t* refusals_of(const ble_addr_t* addr) {
    int free_i = -1;
    for (int i = 0; i < ORBIT_LEDGER_MAX; i++) {
        if (refusals[i].refusals != 0 && ble_addr_cmp(&refusals[i].addr, addr) == 0) {
            return &refusals[i].refusals;
        }
        if (refusals[i].refusals == 0 && free_i < 0) {
            free_i = i;
        }
    }
    if (free_i < 0) {
        free_i = 0;
    }
    refusals[free_i].addr = *addr;
    refusals[free_i].refusals = 0;
    return &refusals[free_i].refusals;
}

// G-1: raise the question for a bonded device (one with a ledger row).
static void ask_approval(dev_t* d, const char* reason) {
    int port = orbit_ledger_port(&d->addr);
    if (port == 0 || approved_for(&d->addr)) {
        return;
    }
    if (approval.wanted && ble_addr_cmp(&approval.addr, &d->addr) == 0) {
        return; // already asking about this device
    }
    if (*refusals_of(&d->addr) >= APPROVAL_MAX_REFUSALS) {
        EVT(d, "%s; not asking again until restart (%d prompts went unanswered)", reason, APPROVAL_MAX_REFUSALS);
        return;
    }
    approval.wanted = true;
    approval.addr = d->addr;
    approval.port = port;
    approval.reason = reason;
    approval.since_us = esp_timer_get_time();
    EVT(d, "approval wanted: %s; press the button within %d s to let port %d pair again", reason,
        (int) (APPROVAL_PROMPT_US / 1000000), port);
    set_last_event("ASK", (int) (d - devs), port);
}

static void clear_approval(void) {
    approval.wanted = false;
    approval.reason = NULL;
}

// Drops the link; the device is excluded for AVOID_US once this has
// happened MAX_FAILS times in a row.
static void give_up_on(dev_t* d, const char* why, int status) {
    bool excluded = fail_note(&d->addr);
    EVT(d, "%s, dropping the link%s", why, excluded ? ", ignoring the device for 30 s" : "");
    set_last_event("ENCFAIL", (int) (d - devs), status);
    ble_gap_terminate(d->conn_handle, BLE_ERR_AUTH_FAIL);
}

static void wake(void) {
    if (wake_task != NULL) {
        xTaskNotifyGive(wake_task);
    }
}

static dev_t* dev_by_handle(uint16_t conn_handle) {
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].connected && devs[i].conn_handle == conn_handle) {
            return &devs[i];
        }
    }
    return NULL;
}

static bool addr_in_use(const ble_addr_t* addr) {
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].in_use && ble_addr_cmp(&devs[i].addr, addr) == 0) {
            return true;
        }
    }
    return false;
}

static dev_t* free_slot(void) {
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (!devs[i].in_use) {
            return &devs[i];
        }
    }
    return NULL;
}

// "public", "static", "rpa" (resolvable private, changes over time) or
// "nrpa". Bonded reconnection through the controller's accept list needs a
// public or static address, so the kind is logged for each bond and link
// (ble-connect-plan.md, stage 1).
static const char* addr_kind(const ble_addr_t* a) {
    switch (a->type) {
    case BLE_ADDR_PUBLIC:
    case BLE_ADDR_PUBLIC_ID:
        return "public";
    case BLE_ADDR_RANDOM:
    case BLE_ADDR_RANDOM_ID:
        return BLE_ADDR_IS_RPA(a) ? "rpa" : BLE_ADDR_IS_NRPA(a) ? "nrpa" : "static";
    default:
        return "?";
    }
}

// A fingerprint of the key we hold for addr, to tell afterwards whether
// the link was encrypted with it or a new pairing replaced it. Only
// compared, never logged.
static uint32_t key_tag(const ble_addr_t* addr) {
    struct ble_store_key_sec key = { .peer_addr = *addr, .idx = 0 };
    struct ble_store_value_sec value;
    // As central we encrypt with the key the device distributed (peer_sec).
    // With legacy pairing our own record may hold no LTK at all (a0d8f9e
    // report: IST Trackball read as "no key stored"), so look there first.
    if ((ble_store_read_peer_sec(&key, &value) != 0 || !value.ltk_present) &&
        (ble_store_read_our_sec(&key, &value) != 0 || !value.ltk_present)) {
        return 0;
    }
    uint32_t h = 2166136261u;
    for (int i = 0; i < sizeof(value.ltk); i++) {
        h = (h ^ value.ltk[i]) * 16777619u;
    }
    return h | 1; // never 0
}

// What we hold for this device and what the link says about its security
// (requirement G: a test device never answers encryption with the
// stored key, so show whether the pairing was bonded, Secure Connections
// or legacy, and which keys each side distributed). Key values are never
// printed.
static void log_security(dev_t* d, const char* when) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(d->conn_handle, &desc) == 0) {
        EVT(d, "%s: link enc=%u auth=%u bonded=%u key_size=%u", when, desc.sec_state.encrypted,
            desc.sec_state.authenticated, desc.sec_state.bonded, desc.sec_state.key_size);
    }
    struct ble_store_key_sec key = { .peer_addr = d->addr, .idx = 0 };
    struct ble_store_value_sec v;
    if (ble_store_read_peer_sec(&key, &v) == 0) {
        EVT(d, "%s: stored peer keys ltk=%u irk=%u csrk=%u sc=%u auth=%u key_size=%u ediv_rand=%s", when,
            v.ltk_present, v.irk_present, v.csrk_present, v.sc, v.authenticated, v.key_size,
            (v.ediv != 0 || v.rand_num != 0) ? "set" : "zero");
    } else {
        EVT(d, "%s: no stored peer keys", when);
    }
    if (ble_store_read_our_sec(&key, &v) == 0) {
        EVT(d, "%s: stored our keys ltk=%u irk=%u csrk=%u sc=%u auth=%u key_size=%u", when, v.ltk_present,
            v.irk_present, v.csrk_present, v.sc, v.authenticated, v.key_size);
    } else {
        EVT(d, "%s: no stored our keys", when);
    }
}

static void restart_discovery_if_unsubscribed(dev_t* d);

// The link is encrypted: say with which key, guard against pairing outside
// pairing mode, and go on to discovery. Reached from the ENC_CHANGE event,
// or from the connect event when the device encrypted the link on its own
// before NimBLE posted the connect event (ff8e166 report, a test device:
// its Security Request right after connecting made NimBLE restore the
// stored key and post ENC_CHANGE while we did not know the link yet, so
// the event was lost; our own encryption request on the already-encrypted
// link then hung until we dropped it).
static void on_encrypted(dev_t* d, const char* when) {
    d->encrypted = true;
    uint32_t now_tag = key_tag(&d->addr);
    const char* how = now_tag == 0              ? "no key stored"
                      : d->key_tag == 0         ? "new pairing"
                      : now_tag == d->key_tag   ? "stored key"
                                                : "new pairing, replaced the stored key";
    EVT(d, "encryption on (%s)%s%s", how, peers_only ? "" : " pairing mode", when);
    if (peers_only && now_tag != d->key_tag) {
        if (approved_for(&d->addr)) {
            // G-2: the user allowed this one device to pair again.
            EVT(d, "new key accepted (approved by the user)");
            approved.until_us = 0;
            *refusals_of(&d->addr) = 0;
        } else {
            // Nothing should pair outside pairing mode (3c46caa, b8d689d).
            EVT(d, "WARNING: paired outside pairing mode");
        }
    }
    d->key_tag = now_tag;
    peers_only = true; // as upstream: back to bonded devices only after a successful pairing
    pairing_since_us = 0;
    int port = ensure_row(d);
    if (port != 0) {
        orbit_ledger_touch(port);
        orbit_ledger_set_key(port, true);
    }
    if (d->discovering) {
        // Subscribing before encryption may have failed; retry once it is up.
        if (d->discovery_finished) {
            restart_discovery_if_unsubscribed(d);
        } else {
            d->rediscover = true;
        }
    }
    start_discovery(d);
}

static void print_params(dev_t* d, const char* what) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(d->conn_handle, &desc) == 0) {
        EVT(d, "%s itvl=%u(%.2fms) lat=%u to=%u(%ums)", what, desc.conn_itvl, ms(desc.conn_itvl), desc.conn_latency,
            desc.supervision_timeout, desc.supervision_timeout * 10);
    }
}

// 1-based position of addr in the bond list; 0 if not bonded (upstream's hub_port).
static int bond_index(const ble_addr_t* addr) {
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0;
    if (ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS) != 0) {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (ble_addr_cmp(&peers[i], addr) == 0) {
            return i + 1;
        }
    }
    return 0;
}

// The ledger row of this device, made when a new device has just bonded
// (pairing mode). Returns its port, 0 when not bonded yet or the ledger is
// full.
static int ensure_row(dev_t* d) {
    int port = orbit_ledger_port(&d->addr);
    if (port != 0 || !is_bonded(&d->addr)) {
        return port;
    }
    port = orbit_ledger_add(&d->addr, true);
    if (port == 0) {
        EVT(d, "ledger full, no port for this device");
    } else {
        EVT(d, "ledger: new device, port %d", port);
    }
    return port;
}

static const struct ble_gap_conn_params conn_params = {
    .scan_itvl = 0x60,   // 60 ms: how often the controller listens while waiting
    .scan_window = 0x30, // 30 ms
    .itvl_min = CONN_ITVL,
    .itvl_max = CONN_ITVL,
    .latency = CONN_LATENCY,
    .supervision_timeout = CONN_TIMEOUT,
    .min_ce_len = 0,
    .max_ce_len = 0,
};

// Pairing mode: look for any HID device (and bonded ones) ourselves.
static void start_scan(void) {
    // Active scan: some devices only put the HID UUID in the scan response.
    // No duplicate filtering: the controller would report each address once
    // per scan, so a device skipped while it was being avoided (or while it
    // was busy) would never be seen again until the scan restarted (0a0cfcf
    // report: a bonded keyboard stayed invisible for an hour).
    // 30 ms of every 60 ms when nothing is connected. With a device
    // connected, the scan took the radio away from it: the IST Trackball's
    // reports fell from 100/s to near 0 while pairing another device
    // (230e758 report), so then listen 10 ms of every 100 ms; pairing
    // takes a little longer.
    bool busy = orbit_ble_connected_count() > 0;
    const struct ble_gap_disc_params params = {
        .itvl = busy ? 0xA0 : 0x60,   // 100 ms : 60 ms
        .window = busy ? 0x10 : 0x30, // 10 ms : 30 ms
        .filter_duplicates = 0,
    };
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc == 0) {
        scanning = true;
        olog("M1 EVT t=%.3f scan start (pairing, listening %s)\n", now_s(), busy ? "10/100 ms" : "30/60 ms");
    } else if (rc != last_scan_rc) {
        // Retried every second by periodic_check(), so only report changes.
        olog("M1 EVT t=%.3f scan start failed rc=0x%x hci=0x%02x\n", now_s(), rc, hci(rc));
    }
    last_scan_rc = rc;
}

// Normal mode: hand the bonded addresses that are not connected to the
// controller's accept list and ask it to connect to whichever shows up
// first (HOGP 5.2.2; what Linux, Zephyr and RMK do). No advertisement
// reaches us, so nothing here can miss one. One round lasts
// CONNECT_TIMEOUT_MS; then NimBLE cancels, the connect event reports the
// timeout, and schedule() starts the next round.
static void start_wl_wait(void) {
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    ble_addr_t list[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0, m = 0;
    ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS);
    for (int i = 0; i < n; i++) {
        if (addr_in_use(&peers[i]) || avoided(&peers[i])) {
            continue;
        }
        list[m] = peers[i];
        // The accept list takes the plain public/random types.
        list[m].type &= 1;
        m++;
    }
    if (m == 0) {
        return; // everything bonded is connected or excluded for now
    }
    int rc = ble_gap_wl_set(list, m);
    if (rc != 0) {
        olog("M1 EVT t=%.3f accept list set failed rc=0x%x\n", now_s(), rc);
        return;
    }
    bool fast = esp_timer_get_time() < fast_until_us;
    struct ble_gap_conn_params params = conn_params;
    if (!fast) {
        params.scan_itvl = 0x1E0; // 300 ms
    }
    rc = ble_gap_connect(own_addr_type, NULL, CONNECT_TIMEOUT_MS, &params, gap_event, NULL);
    if (rc != 0) {
        if (rc != last_scan_rc) {
            olog("M1 EVT t=%.3f accept list wait failed rc=0x%x hci=0x%02x\n", now_s(), rc, hci(rc));
        }
        last_scan_rc = rc;
        return;
    }
    last_scan_rc = 0;
    wl_waiting = true;
    connecting_since_us = esp_timer_get_time();
    connecting_cancel_logged = false;
    if (wait_since_us == 0) {
        wait_since_us = connecting_since_us;
    }
    // Rounds restart every CONNECT_TIMEOUT_MS; only say so when something changed.
    if (m != wait_logged_n || fast != wait_logged_fast) {
        olog("M1 EVT t=%.3f waiting for %d bonded device(s), listening %s\n", now_s(), m,
             fast ? "30/60 ms" : "30/300 ms");
        wait_logged_n = m;
        wait_logged_fast = fast;
    }
}

// Decides what the radio should do now. Called after every event; the
// controller runs at most one of: waiting on the accept list, scanning,
// connecting. Nothing new starts while a link is being discovered, as
// nRF Desktop does.
static void schedule(void) {
    if (connecting || wl_waiting || scanning || free_slot() == NULL) {
        return;
    }
    if (esp_timer_get_time() < hold_until_us) {
        return; // resume_co fires schedule() when the hold ends
    }
    // Only discovery holds the radio. Holding during encryption too made a
    // device whose key fails block the other device for the 5 s ENC_WAIT_US
    // (a0d8f9e report); the one-at-a-time SM limit is handled by the
    // sec_pending retry instead.
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        if (d->connected && d->discovering && !d->discovery_finished) {
            return;
        }
    }
    if (peers_only) {
        start_wl_wait();
    } else {
        start_scan();
    }
}

static void resume(struct ble_npl_event* ev) {
    schedule();
}

static void periodic_check(struct ble_npl_event* ev) {
    int64_t now = esp_timer_get_time();

    if ((connecting || wl_waiting) && now - connecting_since_us > CONNECT_STUCK_RESET_US) {
        olog("M1 EVT t=%.3f connect attempt stuck for %d s, resetting the BLE host\n", now_s(),
             (int) ((now - connecting_since_us) / 1000000));
        ble_hs_sched_reset(BLE_HS_ETIMEOUT);
        return;
    }
    if ((connecting || wl_waiting) && now - connecting_since_us > CONNECT_STUCK_CANCEL_US &&
        !connecting_cancel_logged) {
        int rc = ble_gap_conn_cancel();
        olog("M1 EVT t=%.3f connect attempt stuck for %d s, cancel rc=0x%x\n", now_s(),
             (int) ((now - connecting_since_us) / 1000000), rc);
        connecting_cancel_logged = true;
    }

    if (approval.wanted && now - approval.since_us > APPROVAL_PROMPT_US) {
        uint8_t* n = refusals_of(&approval.addr);
        (*n)++;
        olog("M1 EVT t=%.3f approval for port %d not given within %d s (%u of %d); the device stays out\n", now_s(),
             approval.port, (int) (APPROVAL_PROMPT_US / 1000000), *n, APPROVAL_MAX_REFUSALS);
        set_last_event("NOASK", 0, approval.port);
        clear_approval();
    }
    if (approved.until_us != 0 && now > approved.until_us) {
        olog("M1 EVT t=%.3f approval window closed without a new pairing\n", now_s());
        approved.until_us = 0;
    }

    if (!peers_only && pairing_since_us != 0 && now - pairing_since_us > PAIRING_TIMEOUT_US &&
        orbit_ledger_count() > 0) {
        olog("M1 EVT t=%.3f pairing mode ended after %d s without a new device\n", now_s(),
             (int) (PAIRING_TIMEOUT_US / 1000000));
        stop_pairing_ev(NULL);
    }

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        if (d->connected && d->sec_pending) {
            int rc = ble_gap_security_initiate(d->conn_handle);
            if (rc == 0 || rc == BLE_HS_EALREADY) {
                d->sec_pending = false;
                d->sec_started_us = now;
                EVT(d, "security started");
            } else if (now - d->connected_us > SEC_PENDING_MAX_US) {
                // The other device's procedure never finished; do not hold
                // this slot forever.
                give_up_on(d, "could not start encryption for 15 s", rc);
            }
        }
    }

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        if (d->connected && d->discovering && !d->discovery_finished &&
            now - d->discovery_progress_us > DISCOVERY_STALL_US) {
            d->discovery_finished = true; // so this fires once per link
            d->stalls++;
            if (d->stalls == 1) {
                EVT(d, "discovery stalled for %d s, dropping the link to retry", DISCOVERY_STALL_US / 1000000);
                ble_gap_terminate(d->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            } else {
                // Reconnecting every 5 s never got an answer out of a
                // reset MD600 (f102c45 report: 10 rounds in a minute),
                // while a link left alone recovered after the 30 s ATT
                // timeout (a195bc8). Second stall in a row: keep the
                // link and let the ATT timeout end the discovery.
                EVT(d, "discovery stalled again (%u in a row), keeping the link until the ATT timeout", d->stalls);
            }
        }
    }

    // A device that neither encrypts nor asks to pair within ENC_WAIT_US
    // (one that lost its key but stays silent) would otherwise hold the slot
    // until the 30 s SM timeout (0a0cfcf report). HOGP needs encryption, so
    // an unencrypted link is of no use anyway.
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        // NimBLE does not always post ENC_CHANGE. When the controller's
        // Encryption Change event arrives and the security manager holds no
        // procedure for the link, ble_sm_enc_event_rx() records the link
        // as encrypted (bonded and key_size stay 0) but
        // ble_sm_process_result() leaves its loop before posting any event
        // (the "if (proc == NULL) break;" in ble_sm.c). That is what the
        // test device hits when its Security Request straddles the
        // connect event (9309d56 and f2093b6 reports: enc=1 bonded=0
        // key_size=0, no event). Why its procedure is gone by then is not
        // known; nothing is left pending in the SM either way. So look at
        // the link ourselves once a second while waiting.
        if (d->connected && !d->encrypted && !d->sec_pending) {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(d->conn_handle, &desc) == 0) {
                EVT(d, "waiting for encryption: enc=%u bonded=%u key_size=%u", desc.sec_state.encrypted,
                    desc.sec_state.bonded, desc.sec_state.key_size);
                if (desc.sec_state.encrypted) {
                    // NimBLE did not tie this encryption to a procedure of
                    // its own (bonded and key_size stay 0 then), so it never
                    // posted ENC_CHANGE. Outside pairing mode the link can
                    // only be encrypted with the key we hold for this bonded
                    // address: NimBLE pairs a bonded peer afresh only after
                    // that key has been used (security elevation), or on
                    // REPEAT_PAIRING, which we refuse. So accept it when the
                    // address is bonded and the stored key is unchanged.
                    uint32_t now_tag = key_tag(&d->addr);
                    bool may_pair = !peers_only || approved_for(&d->addr);
                    if (!is_bonded(&d->addr) || now_tag == 0) {
                        // Encrypted, but no key stored yet: a pairing is
                        // still running (the keys come last). Accepting it
                        // here ended pairing mode early, and the keys
                        // stored a moment later then read as "replaced
                        // outside pairing mode" (230e758 and 302af13
                        // reports). Wait for its ENC_CHANGE, or for the
                        // keys to appear here.
                        if (!may_pair) {
                            ask_approval(d, "device paired with a new key");
                            give_up_on(d, "encrypted link with no bond outside pairing mode", 0);
                        }
                        continue;
                    }
                    if (now_tag != d->key_tag && !(may_pair && d->key_tag == 0)) {
                        // The key changed under us: only a new pairing does that.
                        if (!may_pair) {
                            ask_approval(d, "device paired with a new key");
                            give_up_on(d, "encrypted link with a changed key outside pairing mode", 0);
                        }
                        continue;
                    }
                    on_encrypted(d, d->key_tag == 0 ? ", new pairing found by polling (NimBLE posted no event)"
                                                    : ", found by polling (NimBLE posted no event)");
                    continue;
                }
            }
        }
        // Counted from our own request, not from the connect event: while
        // another device's procedure keeps NimBLE busy (rc=0x6) this device
        // has not been asked anything yet (ff8e166 report, problem 2).
        if (d->connected && !d->encrypted && !d->sec_pending && d->sec_started_us != 0 &&
            now - d->sec_started_us > ENC_WAIT_US) {
            log_security(d, "no answer");
            if ((!peers_only || approved_for(&d->addr)) && is_bonded(&d->addr)) {
                // Pairing mode (or approved): the stored key got no answer
                // at all. Forget it so that the reconnect pairs afresh
                // (a0d8f9e report).
                EVT(d, "no encryption after 5 s, forgetting the stored key (%s)",
                    peers_only ? "approved" : "pairing mode");
                ble_store_util_delete_peer(&d->addr);
            } else if (peers_only && is_bonded(&d->addr)) {
                ask_approval(d, "no answer to the stored key");
            }
            give_up_on(d, "no encryption after 5 s", 0);
        }
    }
    schedule();
}

static void pair_new_device_ev(struct ble_npl_event* ev) {
    if (orbit_ledger_full()) {
        // J1: never overwrite a bond silently; the user forgets one first.
        olog("M1 EVT t=%.3f pair_new_device refused: ledger full (%d devices), forget one first\n", now_s(),
             ORBIT_LEDGER_MAX);
        bonds_full_seen = true;
        set_last_event("FULL", 0, -1);
        return;
    }
    olog("M1 EVT t=%.3f pair_new_device\n", now_s());
    peers_only = false;
    pairing_since_us = esp_timer_get_time();
    if (wl_waiting) {
        ble_gap_conn_cancel(); // the connect event that follows leads to schedule()
    } else {
        schedule();
    }
}

static void stop_pairing_ev(struct ble_npl_event* ev) {
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0;
    ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS);
    if (n == 0) {
        olog("M1 EVT t=%.3f stop_pairing: nothing bonded, staying in pairing mode\n", now_s());
        return;
    }
    olog("M1 EVT t=%.3f stop_pairing bonds=%d\n", now_s(), n);
    peers_only = true;
    pairing_since_us = 0;
    if (scanning) {
        int rc = ble_gap_disc_cancel();
        if (rc != 0 && rc != BLE_HS_EALREADY) {
            olog("M1 EVT t=%.3f scan cancel failed rc=0x%x\n", now_s(), rc);
            return;
        }
        scanning = false;
    }
    schedule();
}

static void clear_bonds_on_host(struct ble_npl_event* ev) {
    int rc = ble_store_clear();
    olog("M1 EVT t=%.3f clear_bonds rc=0x%x\n", now_s(), rc);
    orbit_ledger_row_t row;
    while (orbit_ledger_nth(0, &row)) {
        orbit_ledger_forget(row.port);
    }
    bonds_full_seen = false;
    pairing_since_us = esp_timer_get_time();
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].connected) {
            ble_gap_terminate(devs[i].conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }
    peers_only = false;
    if (wl_waiting) {
        ble_gap_conn_cancel();
    }
}

static bool is_bonded(const ble_addr_t* addr) {
    return bond_index(addr) != 0;
}

static void drop_links_of(const ble_addr_t* addr, const char* why) {
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].connected && ble_addr_cmp(&devs[i].addr, addr) == 0) {
            EVT(&devs[i], "%s, dropping the link", why);
            ble_gap_terminate(devs[i].conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }
}

// The accept list is built from the bond list at the start of each round;
// end the current round so that the next one sees the change.
static void restart_wait(void) {
    if (wl_waiting) {
        ble_gap_conn_cancel(); // the connect event that follows leads to schedule()
    } else {
        schedule();
    }
}

static void run_commands(struct ble_npl_event* ev) {
    cmd_t c;
    while (xQueueReceive(cmd_q, &c, 0) == pdTRUE) {
        orbit_ledger_row_t row, old;
        switch (c.kind) {
        case CMD_FORGET:
            if (!orbit_ledger_get(c.a, &row)) {
                olog("M1 EVT t=%.3f forget: no port %d\n", now_s(), c.a);
                break;
            }
            olog("M1 EVT t=%.3f forget port %d addr=..:%02x:%02x\n", now_s(), c.a, row.addr.val[1], row.addr.val[0]);
            drop_links_of(&row.addr, "forgotten");
            ble_store_util_delete_peer(&row.addr);
            orbit_ledger_forget(c.a);
            fail_clear(&row.addr);
            bonds_full_seen = false;
            restart_wait();
            break;
        case CMD_MOVE:
            // The new device (port a) becomes port b; b's row and bond go.
            if (!orbit_ledger_get(c.a, &row) || !orbit_ledger_get(c.b, &old) || c.a == c.b) {
                olog("M1 EVT t=%.3f move: ports %d -> %d not valid\n", now_s(), c.a, c.b);
                break;
            }
            olog("M1 EVT t=%.3f move: port %d addr=..:%02x:%02x takes over port %d addr=..:%02x:%02x\n", now_s(), c.a,
                 row.addr.val[1], row.addr.val[0], c.b, old.addr.val[1], old.addr.val[0]);
            drop_links_of(&old.addr, "replaced");
            ble_store_util_delete_peer(&old.addr);
            orbit_ledger_move(c.a, c.b);
            // The core learned port a with the report map; it hears the new
            // number when the device reconnects.
            drop_links_of(&row.addr, "port changed");
            restart_wait();
            break;
        }
    }
}

// Device names are shown and logged as they come, so anything outside
// printable ASCII becomes '?' and long names are cut.
static void copy_printable(char* out, size_t out_len, const uint8_t* in, size_t in_len) {
    size_t n = 0;
    for (; n < in_len && n + 1 < out_len; n++) {
        out[n] = (in[n] >= 0x20 && in[n] < 0x7F) ? (char) in[n] : '?';
    }
    out[n] = '\0';
}

// A keyboard waking from sleep may advertise without the HID UUID
// (prior-art.md, esp32-hid-gamepad-bridge §4.20), so bonded addresses and
// directed advertising count as well.
// G-2: the button was pressed (or the config tool sent approve).
static void approve_ev_fn(struct ble_npl_event* ev) {
    if (!approval.wanted) {
        olog("M1 EVT t=%.3f approve: nothing is waiting for approval\n", now_s());
        return;
    }
    approved.addr = approval.addr;
    approved.until_us = esp_timer_get_time() + APPROVAL_WINDOW_US;
    olog("M1 EVT t=%.3f approved: port %d addr=..:%02x:%02x may pair again within %d s\n", now_s(), approval.port,
         approval.addr.val[1], approval.addr.val[0], (int) (APPROVAL_WINDOW_US / 1000000));
    set_last_event("OK", 0, approval.port);
    fail_clear(&approval.addr); // it may have been excluded for 30 s; let it back in now
    clear_approval();
    restart_wait();
}

static bool is_candidate(const struct ble_gap_disc_desc* disc) {
    if (avoided(&disc->addr)) {
        return false;
    }
    if (disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND || is_bonded(&disc->addr)) {
        return true;
    }
    if (peers_only) {
        return false;
    }
    if (orbit_ledger_full()) {
        if (!bonds_full_seen) {
            olog("M1 EVT t=%.3f ledger full (%d devices): not connecting new devices, forget one first\n", now_s(),
                 ORBIT_LEDGER_MAX);
            bonds_full_seen = true;
            set_last_event("FULL", 0, -1);
        }
        return false;
    }
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) != 0) {
        return false;
    }
    for (int i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == UUID_HID_SERVICE) {
            return true;
        }
    }
    // Appearance category 0x0F is "Human Interface Device".
    return fields.appearance_is_present && (fields.appearance >> 6) == 0x0F;
}

static void connect_to(const struct ble_gap_disc_desc* disc) {
    dev_t* d = free_slot();
    if (d == NULL || connecting) {
        return;
    }
    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "scan cancel failed rc=%d", rc);
        return;
    }
    scanning = false;

    uint32_t gen = d->gen + 1;
    bool same_device = ble_addr_cmp(&d->addr, &disc->addr) == 0;
    portENTER_CRITICAL(&stats_mux);
    memset(d, 0, offsetof(dev_t, total_reports)); // keep the slot's running totals
    d->gen = gen;
    d->addr = disc->addr;
    if (!same_device) {
        d->stalls = 0;
    }
    portEXIT_CRITICAL(&stats_mux);
    d->ctx = new_gatt_ctx(d);

    rc = ble_gap_connect(own_addr_type, &disc->addr, CONNECT_TIMEOUT_MS, &conn_params, gap_event, NULL);
    if (rc != 0) {
        EVT(d, "connect start failed rc=0x%x hci=0x%02x", rc, hci(rc));
        d->in_use = false;
        schedule();
        return;
    }
    d->in_use = true;
    connecting = true;
    connecting_since_us = esp_timer_get_time();
    connecting_cancel_logged = false;
    struct ble_hs_adv_fields fields;
    char name[32] = "";
    unsigned appearance = 0;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0) {
        copy_printable(name, sizeof(name), fields.name, fields.name_len);
        appearance = fields.appearance_is_present ? fields.appearance : 0;
    }
    EVT(d, "connecting rssi=%d adv_type=%u addr_kind=%s adv_name=\"%s\" adv_appearance=0x%04x", disc->rssi,
        disc->event_type, addr_kind(&disc->addr), name, appearance);
}

static dev_t* connecting_slot(void) {
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].in_use && !devs[i].connected) {
            return &devs[i];
        }
    }
    return NULL;
}

// ---- GATT: Report Map, then each Report's reference + CCCD ----

static void restart_discovery_if_unsubscribed(dev_t* d) {
    if (d->subscribed > 0) {
        return;
    }
    EVT(d, "encrypted now, subscribing again");
    d->discovering = false;
    d->discovery_finished = false;
    d->rediscover = false;
    d->n_svcs = 0;
    d->n_chrs = 0;
    start_discovery(d);
}

// ---- Device information (requirement B) ----
//
// Read after the reports are subscribed, so that a slow or absent answer
// costs nothing but the log line. Each read is one Read By Type over the
// whole database; a device without the characteristic answers "attribute
// not found", logged as none.

static const struct {
    uint16_t uuid;
    const char* what;
} info_reads[] = {
    {UUID_DEVICE_NAME, "name"},
    {UUID_APPEARANCE, "appearance"},
    {UUID_MANUFACTURER, "manufacturer"},
    {UUID_MODEL_NUMBER, "model"},
    {UUID_PNP_ID, "pnp_id"},
};

static void read_next_info(dev_t* d);

static int on_info(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    const char* what = info_reads[d->info_step].what;
    if (error->status == 0 && attr->om != NULL) {
        uint8_t v[64];
        int len = OS_MBUF_PKTLEN(attr->om);
        if (len > (int) sizeof(v)) {
            len = sizeof(v);
        }
        os_mbuf_copydata(attr->om, 0, len, v);
        uint16_t uuid = info_reads[d->info_step].uuid;
        if (uuid == UUID_APPEARANCE && len >= 2) {
            d->info_appearance = (uint16_t) (v[0] | (v[1] << 8));
            EVT(d, "info %s=0x%04x", what, d->info_appearance);
        } else if (uuid == UUID_PNP_ID && len >= 7) {
            d->info_vid = (uint16_t) (v[1] | (v[2] << 8));
            d->info_pid = (uint16_t) (v[3] | (v[4] << 8));
            EVT(d, "info %s: vendor_source=%u vid=0x%04x pid=0x%04x version=0x%04x", what, v[0], d->info_vid,
                d->info_pid, (unsigned) (v[5] | (v[6] << 8)));
        } else {
            char text[32];
            copy_printable(text, sizeof(text), v, len);
            EVT(d, "info %s=\"%s\" (%d byte(s))", what, text, len);
            char* dst = uuid == UUID_DEVICE_NAME    ? d->info_name
                        : uuid == UUID_MANUFACTURER ? d->info_manufacturer
                        : uuid == UUID_MODEL_NUMBER ? d->info_model
                                                    : NULL;
            if (dst != NULL) {
                strncpy(dst, text, ORBIT_LEDGER_TEXT_MAX);
                dst[ORBIT_LEDGER_TEXT_MAX] = '\0';
            }
        }
        return 0; // NimBLE reports the end with BLE_HS_EDONE
    }
    if (error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_FOUND)) {
        EVT(d, "info %s: none", what);
    } else if (error->status != BLE_HS_EDONE) {
        EVT(d, "info %s: read failed status=0x%x", what, error->status);
    }
    d->info_step++;
    read_next_info(d);
    return 0;
}

// Hands the report map to the core. Delayed for a new device until its
// ledger row is settled, so that the core hears the final port number.
static void deliver_report_map(dev_t* d) {
    orbit_report_map_t* m = &d->report_map;
    m->hub_port = orbit_ledger_port(&d->addr);
    d->map_pending = false;
    EVT(d, "report map %u byte(s) hash=%08x, hub_port=%u", m->len, (unsigned) d->map_hash, m->hub_port);
    if (m->len > 0 && xQueueSend(report_map_q, m, 0) == pdTRUE) {
        wake();
    }
}

// All reads done: fill the ledger row. A row added in this session is
// compared with the older rows: when exactly one looks like the same
// device and that one is not connected, the new row takes over its port
// (requirement D, decision J3: a device whose address changed after a
// reset). With two or more candidates nothing is replaced; the user picks.
static void info_done(dev_t* d) {
    int port = orbit_ledger_port(&d->addr);
    if (port == 0) {
        if (d->map_pending) {
            deliver_report_map(d);
        }
        return;
    }
    orbit_ledger_set_text(port, d->info_name, d->info_manufacturer, d->info_model);
    orbit_ledger_set_ids(port, d->info_vid, d->info_pid, d->info_appearance);
    orbit_ledger_row_t row;
    if (orbit_ledger_get(port, &row) && (row.flags & ORBIT_LEDGER_NEW)) {
        int similar[ORBIT_LEDGER_MAX];
        int n = orbit_ledger_similar(port, similar, ORBIT_LEDGER_MAX);
        orbit_ledger_row_t old;
        if (n == 1 && orbit_ledger_get(similar[0], &old) && !addr_in_use(&old.addr)) {
            EVT(d, "ledger: looks like port %d (addr=..:%02x:%02x, not connected), taking over its port", similar[0],
                old.addr.val[1], old.addr.val[0]);
            orbit_ledger_move(port, similar[0]);
            ble_store_util_delete_peer(&old.addr);
            port = similar[0];
        } else if (n >= 1) {
            EVT(d, "ledger: %d row(s) look like this device, kept as port %d; forget the old one by hand", n, port);
            duplicates_seen = true;
        }
        orbit_ledger_clear_flag(port, ORBIT_LEDGER_NEW);
    }
    char name[64];
    if (orbit_ledger_get(port, &row)) {
        orbit_ledger_display_name(&row, name, sizeof(name));
        EVT(d, "ledger: port %d \"%s\" kind=%s", port, name, orbit_ledger_kind_name(row.kind));
    }
    if (d->map_pending) {
        deliver_report_map(d);
    }
}

static void bat_start(dev_t* d);

static void read_next_info(dev_t* d) {
    if (d->info_step >= (int) (sizeof(info_reads) / sizeof(info_reads[0]))) {
        info_done(d);
        bat_start(d);
        return;
    }
    int rc = ble_gattc_read_by_uuid(d->conn_handle, 1, 0xFFFF, BLE_UUID16_DECLARE(info_reads[d->info_step].uuid),
                                    on_info, d->ctx);
    if (rc != 0) {
        EVT(d, "info %s: read start failed rc=0x%x", info_reads[d->info_step].what, rc);
    }
}

static void discovery_done(dev_t* d) {
    d->discovery_finished = true;
    if (d->subscribed > 0) {
        d->stalls = 0;
        fail_clear(&d->addr);
        d->info_step = 0;
        read_next_info(d);
    }
    EVT(d, "subscribed %d input report(s)", d->subscribed);
    if (d->rediscover) {
        restart_discovery_if_unsubscribed(d);
        return;
    }
    if (d->subscribed == 0 && d->encrypted) {
        // Discovery ran but found nothing to subscribe to (a failed or
        // interrupted procedure). The link is useless as it is; drop it and
        // let the reconnect discover afresh. Not the device's fault, so no
        // 30 s avoidance.
        EVT(d, "nothing subscribed, dropping the link to retry");
        ble_gap_terminate(d->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    schedule(); // this link is set up; the radio may look for the next device
}

static int on_cccd_written(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr,
                           void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    chr_t* c = &d->chrs[d->cur_chr];
    if (error->status == 0) {
        d->subscribed++;
        EVT(d, "input report id=%u handle=%u subscribed", c->report_id, c->val_handle);
    } else {
        EVT(d, "cccd write failed handle=%u status=0x%x", c->cccd_handle, error->status);
    }
    d->cur_chr++;
    discover_next_report(d);
    return 0;
}

static int on_report_ref(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr,
                         void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    chr_t* c = &d->chrs[d->cur_chr];
    if (error->status == 0 && attr->om != NULL && OS_MBUF_PKTLEN(attr->om) >= 2) {
        uint8_t ref[2];
        os_mbuf_copydata(attr->om, 0, 2, ref);
        c->report_id = ref[0];
        c->report_type = ref[1];
    } else {
        EVT(d, "report reference read failed handle=%u status=0x%x", c->ref_handle, error->status);
        c->report_type = 0;
    }
    if (c->report_type == REPORT_TYPE_INPUT && (c->props & BLE_GATT_CHR_PROP_NOTIFY) && c->cccd_handle != 0) {
        static const uint8_t notify_on[2] = { 0x01, 0x00 };
        int rc = ble_gattc_write_flat(conn_handle, c->cccd_handle, notify_on, sizeof(notify_on), on_cccd_written, d->ctx);
        if (rc == 0) {
            return 0;
        }
        EVT(d, "cccd write start failed rc=0x%x", rc);
    }
    d->cur_chr++;
    discover_next_report(d);
    return 0;
}

// The 16-bit value of a characteristic or descriptor UUID, or 0. Some
// devices send 16-bit UUIDs as 128-bit ones with an all-zero base
// (0000xxxx-0000-0000-0000-000000000000) instead of the Bluetooth base;
// upstream's BLE firmware reads those as 16-bit too (patch_broken_uuids()
// in firmware-bluetooth/src/main.cc), and so do we.
static uint16_t uuid16_of(dev_t* d, const ble_uuid_any_t* uuid) {
    if (uuid->u.type == BLE_UUID_TYPE_16) {
        return uuid->u16.value;
    }
    if (uuid->u.type != BLE_UUID_TYPE_128) {
        return 0;
    }
    const uint8_t* v = uuid->u128.value; // little-endian, the 16-bit part at 12..13
    for (int i = 0; i < 16; i++) {
        if (i != 12 && i != 13 && v[i] != 0) {
            return 0;
        }
    }
    uint16_t value = v[13] << 8 | v[12];
    EVT(d, "broken 128-bit UUID read as 0x%04x", value);
    return value;
}

static int on_dsc(uint16_t conn_handle, const struct ble_gatt_error* error, uint16_t chr_val_handle,
                  const struct ble_gatt_dsc* dsc, void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    chr_t* c = &d->chrs[d->cur_chr];
    if (error->status == 0) {
        uint16_t uuid16 = uuid16_of(d, &dsc->uuid);
        if (uuid16 == UUID_CCCD) {
            c->cccd_handle = dsc->handle;
        } else if (uuid16 == UUID_REPORT_REF) {
            c->ref_handle = dsc->handle;
        }
        return 0;
    }
    if (error->status == BLE_HS_EDONE && c->ref_handle != 0) {
        int rc = ble_gattc_read(conn_handle, c->ref_handle, on_report_ref, d->ctx);
        if (rc == 0) {
            return 0;
        }
        EVT(d, "report reference read start failed rc=0x%x", rc);
    } else if (error->status != BLE_HS_EDONE) {
        EVT(d, "descriptor discovery failed status=0x%x", error->status);
    } else {
        EVT(d, "report handle=%u has no report reference, skipped", c->val_handle);
    }
    d->cur_chr++;
    discover_next_report(d);
    return 0;
}

static void discover_next_report(dev_t* d) {
    for (; d->cur_chr < d->n_chrs; d->cur_chr++) {
        chr_t* c = &d->chrs[d->cur_chr];
        if (c->uuid16 != UUID_REPORT || c->end_handle <= c->val_handle) {
            continue;
        }
        c->cccd_handle = 0;
        c->ref_handle = 0;
        int rc = ble_gattc_disc_all_dscs(d->conn_handle, c->val_handle, c->end_handle, on_dsc, d->ctx);
        if (rc == 0) {
            return;
        }
        EVT(d, "descriptor discovery start failed rc=0x%x", rc);
    }
    discovery_done(d);
}

static int on_report_map(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr,
                         void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    orbit_report_map_t* m = &d->report_map;
    if (error->status == 0 && attr->om != NULL) {
        int len = OS_MBUF_PKTLEN(attr->om);
        if (attr->offset + len <= ORBIT_REPORT_MAP_MAX) {
            os_mbuf_copydata(attr->om, 0, len, m->data + attr->offset);
            m->len = attr->offset + len;
        } else {
            EVT(d, "report map longer than %d bytes, truncated", ORBIT_REPORT_MAP_MAX);
        }
        return 0; // NimBLE keeps reading until the value ends
    }
    if (error->status != BLE_HS_EDONE) {
        EVT(d, "report map read failed status=0x%x, %u byte(s) discarded", error->status, m->len);
        m->len = 0; // a partial descriptor must not reach the core
    }
    m->interface = (uint16_t) ((d - devs) << 8);
    uint32_t hash = 2166136261u; // FNV-1a: tells report maps apart in the log without dumping them
    for (unsigned i = 0; i < m->len; i++) {
        hash = (hash ^ m->data[i]) * 16777619u;
    }
    d->map_hash = hash;
    int port = ensure_row(d);
    orbit_ledger_row_t row;
    bool is_new = port != 0 && orbit_ledger_get(port, &row) && (row.flags & ORBIT_LEDGER_NEW);
    if (port != 0 && m->len > 0) {
        orbit_ledger_set_map(port, hash, orbit_ledger_kind_of_map(m->data, m->len));
    }
    if (is_new) {
        d->map_pending = true; // the port may still change in info_done()
        EVT(d, "report map %u byte(s) hash=%08x, held until the ledger row is settled", m->len, (unsigned) hash);
    } else {
        deliver_report_map(d);
    }
    d->cur_chr = 0;
    discover_next_report(d);
    return 0;
}

static void discover_next_svc_chrs(dev_t* d);

static int on_chr(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_chr* chr,
                  void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    if (error->status == 0) {
        if (d->n_chrs < MAX_CHRS) {
            chr_t* c = &d->chrs[d->n_chrs++];
            memset(c, 0, sizeof(*c));
            c->def_handle = chr->def_handle;
            c->val_handle = chr->val_handle;
            c->end_handle = d->svc_end[d->cur_svc];
            c->props = chr->properties;
            c->uuid16 = uuid16_of(d, &chr->uuid);
            if (c->uuid16 == UUID_REPORT_MAP && d->report_map_handle == 0) {
                d->report_map_handle = c->val_handle;
            }
            // Descriptors of the previous characteristic end right before this one.
            if (d->n_chrs >= 2 && d->chrs[d->n_chrs - 2].end_handle == c->end_handle) {
                d->chrs[d->n_chrs - 2].end_handle = c->def_handle - 1;
            }
        } else {
            EVT(d, "too many characteristics, ignoring handle %u", chr->val_handle);
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        EVT(d, "characteristic discovery failed status=0x%x", error->status);
    }
    d->cur_svc++;
    discover_next_svc_chrs(d);
    return 0;
}

static void discover_next_svc_chrs(dev_t* d) {
    if (d->cur_svc < d->n_svcs) {
        int rc = ble_gattc_disc_all_chrs(d->conn_handle, d->svc_start[d->cur_svc], d->svc_end[d->cur_svc], on_chr, d->ctx);
        if (rc == 0) {
            return;
        }
        EVT(d, "characteristic discovery start failed rc=0x%x", rc);
    }
    // All characteristics known: read the Report Map, then set up each Report.
    d->report_map.len = 0;
    if (d->report_map_handle != 0) {
        int rc = ble_gattc_read_long(d->conn_handle, d->report_map_handle, 0, on_report_map, d->ctx);
        if (rc == 0) {
            return;
        }
        EVT(d, "report map read start failed rc=0x%x", rc);
    } else {
        EVT(d, "no report map characteristic");
    }
    d->cur_chr = 0;
    discover_next_report(d);
}

static int on_svc(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_svc* svc,
                  void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    if (error->status == 0) {
        if (d->n_svcs < MAX_HID_SVCS) {
            d->svc_start[d->n_svcs] = svc->start_handle;
            d->svc_end[d->n_svcs] = svc->end_handle;
            d->n_svcs++;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        EVT(d, "service discovery failed status=0x%x", error->status);
    }
    EVT(d, "found %d HID service(s)", d->n_svcs);
    d->cur_svc = 0;
    d->report_map_handle = 0;
    discover_next_svc_chrs(d);
    return 0;
}

static void start_discovery(dev_t* d) {
    if (d->discovering) {
        return;
    }
    d->discovering = true;
    d->discovery_progress_us = esp_timer_get_time();
    int rc = ble_gattc_disc_svc_by_uuid(d->conn_handle, BLE_UUID16_DECLARE(UUID_HID_SERVICE), on_svc, d->ctx);
    if (rc != 0) {
        EVT(d, "service discovery start failed rc=0x%x", rc);
    }
}

// ---- Battery Service (stage 0: what the devices offer) ----
//
// Runs once per link, after the device information, so that input flows
// first. One GATT procedure at a time, as everywhere here. For each Battery
// Level characteristic: its descriptors, its value, its User Description
// and Presentation Format (how a split keyboard tells its halves apart),
// then notifications when it offers them. Everything goes to the log; the
// screen does not show it yet.

enum { BAT_SVCS, BAT_CHRS, BAT_DSCS, BAT_LEVEL, BAT_USER_DESC, BAT_PRESENTATION, BAT_CCCD, BAT_DONE };

static void bat_next(dev_t* d);

static void bat_advance(dev_t* d) {
    if (d->bat_phase == BAT_CCCD) {
        d->bat_cur++;
        d->bat_phase = BAT_DSCS;
    } else {
        d->bat_phase++;
    }
    bat_next(d);
}

static int on_bat_svc(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_svc* svc,
                      void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0;
    }
    if (error->status == 0) {
        if (d->bat_n_svcs < MAX_BAT) {
            d->bat_svc_start[d->bat_n_svcs] = svc->start_handle;
            d->bat_svc_end[d->bat_n_svcs] = svc->end_handle;
            d->bat_n_svcs++;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE && error->status != BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_FOUND)) {
        EVT(d, "battery: service discovery failed status=0x%x", error->status);
    }
    EVT(d, "battery: %d Battery Service(s)", d->bat_n_svcs);
    d->bat_cur = 0;
    d->bat_phase = BAT_CHRS;
    bat_next(d);
    return 0;
}

static int on_bat_chr(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_chr* chr,
                      void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0;
    }
    if (error->status == 0) {
        // Descriptors of the previous characteristic end right before this one.
        if (d->bat_n > 0 && d->bat[d->bat_n - 1].end_handle == d->bat_svc_end[d->bat_cur] &&
            d->bat[d->bat_n - 1].val_handle < chr->def_handle) {
            d->bat[d->bat_n - 1].end_handle = chr->def_handle - 1;
        }
        if (uuid16_of(d, &chr->uuid) == UUID_BATTERY_LEVEL && d->bat_n < MAX_BAT) {
            memset(&d->bat[d->bat_n], 0, sizeof(d->bat[0]));
            d->bat[d->bat_n].def_handle = chr->def_handle;
            d->bat[d->bat_n].val_handle = chr->val_handle;
            d->bat[d->bat_n].end_handle = d->bat_svc_end[d->bat_cur];
            d->bat[d->bat_n].props = chr->properties;
            d->bat_n++;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        EVT(d, "battery: characteristic discovery failed status=0x%x", error->status);
    }
    d->bat_cur++;
    bat_next(d); // still BAT_CHRS until the last service
    return 0;
}

static int on_bat_dsc(uint16_t conn_handle, const struct ble_gatt_error* error, uint16_t chr_val_handle,
                      const struct ble_gatt_dsc* dsc, void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0;
    }
    if (error->status == 0) {
        uint16_t uuid16 = uuid16_of(d, &dsc->uuid);
        if (uuid16 == UUID_CCCD) {
            d->bat[d->bat_cur].cccd_handle = dsc->handle;
        } else if (uuid16 == UUID_USER_DESC) {
            d->bat[d->bat_cur].user_desc_handle = dsc->handle;
        } else if (uuid16 == UUID_PRESENTATION) {
            d->bat[d->bat_cur].presentation_handle = dsc->handle;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        EVT(d, "battery %d: descriptor discovery failed status=0x%x", d->bat_cur, error->status);
    }
    bat_chr_t* b = &d->bat[d->bat_cur];
    EVT(d, "battery %d: handle=%u read=%d notify=%d cccd=%d user_desc=%d presentation=%d", d->bat_cur, b->val_handle,
        !!(b->props & BLE_GATT_CHR_PROP_READ), !!(b->props & BLE_GATT_CHR_PROP_NOTIFY), b->cccd_handle != 0,
        b->user_desc_handle != 0, b->presentation_handle != 0);
    bat_advance(d);
    return 0;
}

static int on_bat_read(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr,
                       void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0;
    }
    if (error->status != 0 || attr == NULL || attr->om == NULL) {
        EVT(d, "battery %d: read failed status=0x%x", d->bat_cur, error->status);
        bat_advance(d);
        return 0;
    }
    uint8_t v[32];
    int len = OS_MBUF_PKTLEN(attr->om);
    if (len > (int) sizeof(v)) {
        len = sizeof(v);
    }
    os_mbuf_copydata(attr->om, 0, len, v);
    if (d->bat_phase == BAT_LEVEL) {
        EVT(d, "battery %d: level=%u%% (%d byte(s))", d->bat_cur, len > 0 ? v[0] : 0, len);
    } else if (d->bat_phase == BAT_USER_DESC) {
        char text[32];
        copy_printable(text, sizeof(text), v, len);
        EVT(d, "battery %d: user description=\"%s\"", d->bat_cur, text);
    } else if (len >= 7) { // format(1) exponent(1) unit(2) namespace(1) description(2)
        EVT(d, "battery %d: presentation format=0x%02x unit=0x%04x namespace=0x%02x description=0x%04x", d->bat_cur,
            v[0], (unsigned) (v[2] | v[3] << 8), v[4], (unsigned) (v[5] | v[6] << 8));
    } else {
        EVT(d, "battery %d: presentation format %d byte(s)", d->bat_cur, len);
    }
    bat_advance(d);
    return 0;
}

static int on_bat_cccd(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr,
                       void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0;
    }
    if (error->status == 0) {
        EVT(d, "battery %d: notifications on", d->bat_cur);
    } else {
        EVT(d, "battery %d: cccd write failed status=0x%x", d->bat_cur, error->status);
    }
    bat_advance(d);
    return 0;
}

// Starts the procedure of the current phase, or moves on when it has none.
static void bat_next(dev_t* d) {
    static const uint8_t notify_on[2] = {1, 0};
    for (;;) {
        int rc = 0;
        if (d->bat_phase == BAT_CHRS) {
            if (d->bat_cur >= d->bat_n_svcs) {
                EVT(d, "battery: %d Battery Level characteristic(s)", d->bat_n);
                d->bat_cur = 0;
                d->bat_phase = BAT_DSCS;
                continue;
            }
            rc = ble_gattc_disc_all_chrs(d->conn_handle, d->bat_svc_start[d->bat_cur], d->bat_svc_end[d->bat_cur],
                                         on_bat_chr, d->ctx);
        } else if (d->bat_phase >= BAT_DSCS && d->bat_phase <= BAT_CCCD && d->bat_cur >= d->bat_n) {
            d->bat_phase = BAT_DONE;
            EVT(d, "battery: probe done");
            return;
        } else {
            bat_chr_t* b = &d->bat[d->bat_cur];
            switch (d->bat_phase) {
            case BAT_DSCS:
                if (b->end_handle <= b->val_handle) {
                    EVT(d, "battery %d: handle=%u read=%d notify=%d, no descriptors", d->bat_cur, b->val_handle,
                        !!(b->props & BLE_GATT_CHR_PROP_READ), !!(b->props & BLE_GATT_CHR_PROP_NOTIFY));
                    d->bat_phase++;
                    continue;
                }
                rc = ble_gattc_disc_all_dscs(d->conn_handle, b->val_handle, b->end_handle, on_bat_dsc, d->ctx);
                break;
            case BAT_LEVEL:
                if (!(b->props & BLE_GATT_CHR_PROP_READ)) {
                    d->bat_phase++;
                    continue;
                }
                rc = ble_gattc_read(d->conn_handle, b->val_handle, on_bat_read, d->ctx);
                break;
            case BAT_USER_DESC:
                if (b->user_desc_handle == 0) {
                    d->bat_phase++;
                    continue;
                }
                rc = ble_gattc_read(d->conn_handle, b->user_desc_handle, on_bat_read, d->ctx);
                break;
            case BAT_PRESENTATION:
                if (b->presentation_handle == 0) {
                    d->bat_phase++;
                    continue;
                }
                rc = ble_gattc_read(d->conn_handle, b->presentation_handle, on_bat_read, d->ctx);
                break;
            case BAT_CCCD:
                if (!(b->props & BLE_GATT_CHR_PROP_NOTIFY) || b->cccd_handle == 0) {
                    d->bat_cur++;
                    d->bat_phase = BAT_DSCS;
                    continue;
                }
                rc = ble_gattc_write_flat(d->conn_handle, b->cccd_handle, notify_on, sizeof(notify_on), on_bat_cccd,
                                          d->ctx);
                break;
            default:
                return;
            }
        }
        if (rc == 0) {
            return;
        }
        EVT(d, "battery: phase %d start failed rc=0x%x, probe stopped", d->bat_phase, rc);
        d->bat_phase = BAT_DONE;
        return;
    }
}

static void bat_start(dev_t* d) {
    if (d->bat_started) {
        return;
    }
    d->bat_started = true;
    d->bat_phase = BAT_SVCS;
    int rc = ble_gattc_disc_svc_by_uuid(d->conn_handle, BLE_UUID16_DECLARE(UUID_BATTERY_SERVICE), on_bat_svc, d->ctx);
    if (rc != 0) {
        EVT(d, "battery: service discovery start failed rc=0x%x", rc);
        d->bat_phase = BAT_DONE;
    }
}

// ---- GAP events ----

static void on_notify(dev_t* d, uint16_t attr_handle, struct os_mbuf* om) {
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < d->bat_n; i++) {
        if (d->bat[i].val_handle == attr_handle) {
            uint8_t level = 0;
            int len = OS_MBUF_PKTLEN(om);
            os_mbuf_copydata(om, 0, len > 0 ? 1 : 0, &level);
            EVT(d, "battery %d notify level=%u%% (%d byte(s)) after %.1f s", i, level, len,
                (now - d->connected_us) / 1e6);
            return; // not an input report
        }
    }
    portENTER_CRITICAL(&stats_mux);
    if (d->last_report_us != 0) {
        int64_t gap = now - d->last_report_us;
        if (gap < 1000000) {
            if (gap > d->max_gap_us) {
                d->max_gap_us = (uint32_t) gap;
            }
            d->gaps[gap <= 8000 ? GAP_LE_8MS : gap <= 16000 ? GAP_LE_16MS : gap <= 32000 ? GAP_LE_32MS : GAP_OVER_32MS]++;
        }
    }
    d->last_report_us = now;
    d->reports++;
    d->total_reports++;
    portEXIT_CRITICAL(&stats_mux);

    uint8_t report_id = 0;
    for (int i = 0; i < d->n_chrs; i++) {
        if (d->chrs[i].val_handle == attr_handle) {
            report_id = d->chrs[i].report_id;
            break;
        }
    }
    orbit_report_t r = {
        .t_us = now,
        .interface = (uint16_t) ((d - devs) << 8),
        .report_id = report_id,
        .len = OS_MBUF_PKTLEN(om),
    };
    if (r.len > ORBIT_REPORT_MAX) {
        r.len = ORBIT_REPORT_MAX;
    }
    os_mbuf_copydata(om, 0, r.len, r.data);
    if (xQueueSend(report_q, &r, 0) == pdTRUE) {
        wake();
    } else {
        portENTER_CRITICAL(&stats_mux);
        reports_lost++;
        portEXIT_CRITICAL(&stats_mux);
    }
}

static int on_update_request(dev_t* d, const char* kind, const struct ble_gap_upd_params* peer) {
    d->peer_allows_itvl = peer->itvl_min <= CONN_ITVL && CONN_ITVL <= peer->itvl_max;
    EVT(d, "%s from device itvl=%u-%u(%.2f-%.2fms) lat=%u to=%u -> accept", kind, peer->itvl_min, peer->itvl_max,
        ms(peer->itvl_min), ms(peer->itvl_max), peer->latency, peer->supervision_timeout);
    set_last_event("UPDREQ", (int) (d - devs), peer->itvl_min);
    return 0;
}

static int gap_event(struct ble_gap_event* event, void* arg) {
    dev_t* d;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        if (!addr_in_use(&event->disc.addr) && is_candidate(&event->disc)) {
            connect_to(&event->disc);
        }
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        scanning = false;
        return 0;

    case BLE_GAP_EVENT_CONNECT: {
        if (wl_waiting) {
            wl_waiting = false;
            if (event->connect.status != 0) {
                // The round ended (timeout, or cancelled for pairing mode).
                if (event->connect.status != BLE_HS_ETIMEOUT) {
                    olog("M1 EVT t=%.3f accept list wait ended status=0x%x\n", now_s(), event->connect.status);
                }
                schedule();
                return 0;
            }
            // A bonded device showed up. Its slot: the one it had last time
            // (keeps its totals), else a free one.
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) != 0) {
                ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                schedule();
                return 0;
            }
            d = NULL;
            for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
                if (!devs[i].in_use && ble_addr_cmp(&devs[i].addr, &desc.peer_id_addr) == 0) {
                    d = &devs[i];
                }
            }
            if (d == NULL) {
                d = free_slot();
            }
            if (d == NULL) {
                ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                return 0;
            }
            uint32_t gen = d->gen + 1;
            bool same_device = ble_addr_cmp(&d->addr, &desc.peer_id_addr) == 0;
            portENTER_CRITICAL(&stats_mux);
            memset(d, 0, offsetof(dev_t, total_reports));
            d->gen = gen;
            d->addr = desc.peer_id_addr;
            d->in_use = true;
            if (!same_device) {
                d->stalls = 0;
            }
            portEXIT_CRITICAL(&stats_mux);
            d->ctx = new_gatt_ctx(d);
            // The controller connects on the device's first advertisement,
            // so this is when the device showed up; the wait may span many
            // rounds (1fd6c6a report asked for such a marker).
            EVT(d, "connecting via accept list after %.1f s of waiting",
                (esp_timer_get_time() - wait_since_us) / 1e6);
            wait_since_us = 0;
            wait_logged_n = -1;
        } else {
            connecting = false;
            d = connecting_slot();
            if (d == NULL) {
                return 0;
            }
            if (event->connect.status != 0) {
                EVT(d, "connect failed status=0x%x hci=0x%02x", event->connect.status, hci(event->connect.status));
                set_last_event("CONNFAIL", (int) (d - devs), event->connect.status);
                d->in_use = false;
                schedule();
                return 0;
            }
        }
        portENTER_CRITICAL(&stats_mux);
        d->connected = true;
        d->conn_handle = event->connect.conn_handle;
        d->connected_us = esp_timer_get_time();
        portEXIT_CRITICAL(&stats_mux);
        d->key_tag = key_tag(&d->addr);
        print_params(d, "connected");
        {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(d->conn_handle, &desc) == 0) {
                EVT(d, "peer id=%s ..:%02x:%02x ota=%s", addr_kind(&desc.peer_id_addr), desc.peer_id_addr.val[1],
                    desc.peer_id_addr.val[0], addr_kind(&desc.peer_ota_addr));
            }
        }
        set_last_event("CONN", (int) (d - devs), -1);
        // HOGP devices may not send reports until the link is encrypted
        // (prior-art.md §4.29), so start it ourselves, unless the device
        // already did (see on_encrypted()).
        struct ble_gap_conn_desc sec;
        if (ble_gap_conn_find(d->conn_handle, &sec) == 0 && sec.sec_state.encrypted) {
            on_encrypted(d, ", before the connect event");
            schedule();
            return 0;
        }
        d->sec_started_us = esp_timer_get_time();
        int rc = ble_gap_security_initiate(d->conn_handle);
        if (rc == BLE_HS_EALREADY) {
            // The device started pairing on its own; ours is not needed.
            // Retrying after its pairing succeeds re-encrypts an encrypted
            // link and the device drops it (a0d8f9e report, a test device).
            struct ble_gap_conn_desc now_desc;
            if (ble_gap_conn_find(d->conn_handle, &now_desc) == 0) {
                EVT(d, "device started security itself (enc=%u bonded=%u key_size=%u at this moment)",
                    now_desc.sec_state.encrypted, now_desc.sec_state.bonded, now_desc.sec_state.key_size);
            } else {
                EVT(d, "device started security itself");
            }
        } else if (rc != 0) {
            // NimBLE runs one pairing/encryption procedure at a time
            // (BLE_SM_MAX_PROCS = 1), so this fails with rc=0x6 while the
            // other device is pairing (0a0cfcf report). Not the device's
            // fault: retry from periodic_check().
            EVT(d, "security start failed rc=0x%x, retrying", rc);
            d->sec_pending = true;
        }
        schedule(); // holds while this link is being discovered
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
        d = dev_by_handle(event->disconnect.conn.conn_handle);
        if (d == NULL) {
            // NimBLE reads the peer's features and version before it posts
            // the connect event. If the link dies meanwhile (0x3E, the
            // device did not answer its first packets), the host drops the
            // failed read on the floor assuming its own reattempt follows
            // (ble_gap_rx_rd_rem_sup_feat_complete: "Reconnection will
            // automatically happen"). That reattempt is off (sdkconfig.defaults),
            // and it silently gives up after three tries anyway, so this
            // disconnect for a handle we never saw is the only notice of the
            // failed attempt (f90e28c report: 8 stuck attempts in 20 min).
            const ble_addr_t* peer = &event->disconnect.conn.peer_id_addr;
            if (wl_waiting) {
                // The accept-list wait produced a link that died at once.
                // NimBLE has already left the connect procedure, so the
                // wait is over; start the next round.
                wl_waiting = false;
                olog("M1 EVT t=%.3f addr=..:%02x:%02x connect failed before the connect event reason=0x%x hci=0x%02x\n",
                     now_s(), peer->val[1], peer->val[0], event->disconnect.reason, hci(event->disconnect.reason));
                schedule();
                return 0;
            }
            d = connecting ? connecting_slot() : NULL;
            if (d == NULL || ble_addr_cmp(&d->addr, peer) != 0) {
                olog("M1 EVT t=%.3f disconnect for an unknown link h=%u reason=0x%x\n", now_s(),
                     event->disconnect.conn.conn_handle, event->disconnect.reason);
                return 0;
            }
            EVT(d, "connect failed before the connect event reason=0x%x hci=0x%02x", event->disconnect.reason,
                hci(event->disconnect.reason));
            set_last_event("CONNFAIL", (int) (d - devs), hci(event->disconnect.reason));
            connecting = false;
            portENTER_CRITICAL(&stats_mux);
            d->in_use = false;
            d->gen++;
            portEXIT_CRITICAL(&stats_mux);
            schedule();
            return 0;
        }
        EVT(d, "disconnected reason=0x%x hci=0x%02x", event->disconnect.reason, hci(event->disconnect.reason));
        set_last_event("DISC", (int) (d - devs), hci(event->disconnect.reason));
        portENTER_CRITICAL(&stats_mux);
        d->connected = false;
        d->in_use = false;
        d->gen++;
        d->disconnects++;
        portEXIT_CRITICAL(&stats_mux);
        {
            orbit_disconnect_t item = { .slot = (uint8_t) (d - devs) };
            if (xQueueSend(disconnect_q, &item, 0) == pdTRUE) {
                wake();
            }
        }
        // Let the device settle before the next attempt (RMK waits 0.5 s,
        // upstream's BLE firmware 1 s).
        hold_until_us = esp_timer_get_time() + RECONNECT_HOLD_MS * 1000;
        fast_until_us = esp_timer_get_time() + FAST_WAIT_US;
        wait_since_us = 0;
        wait_logged_n = -1;
        ble_npl_callout_reset(&resume_co, ble_npl_time_ms_to_ticks32(RECONNECT_HOLD_MS));
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        d = dev_by_handle(event->conn_update.conn_handle);
        if (d == NULL) {
            return 0;
        }
        if (event->conn_update.status != 0) {
            EVT(d, "update failed status=0x%x hci=0x%02x", event->conn_update.status, hci(event->conn_update.status));
            set_last_event("UPDFAIL", (int) (d - devs), hci(event->conn_update.status));
        } else {
            print_params(d, "updated");
            set_last_event("UPD", (int) (d - devs), -1);
            struct ble_gap_conn_desc desc;
            if (d->peer_allows_itvl && !d->reasserted && ble_gap_conn_find(d->conn_handle, &desc) == 0 &&
                desc.conn_itvl != CONN_ITVL) {
                // The device's range includes our interval but the update
                // landed elsewhere: ask once more (Q31 60540d8). Expected to
                // be refused for the second device (q31-results.md §5).
                d->reasserted = true;
                const struct ble_gap_upd_params upd = {
                    .itvl_min = CONN_ITVL,
                    .itvl_max = CONN_ITVL,
                    .latency = desc.conn_latency,
                    .supervision_timeout = desc.supervision_timeout,
                };
                int rc = ble_gap_update_params(d->conn_handle, &upd);
                EVT(d, "device allows %.2fms, asking for it again rc=0x%x", ms(CONN_ITVL), rc);
            }
        }
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
    case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
        d = dev_by_handle(event->conn_update_req.conn_handle);
        if (d == NULL) {
            return 0;
        }
        return on_update_request(d, event->type == BLE_GAP_EVENT_CONN_UPDATE_REQ ? "LL update req" : "L2CAP update req",
                                 event->conn_update_req.peer_params);

    case BLE_GAP_EVENT_PARING_COMPLETE:
        d = dev_by_handle(event->pairing_complete.conn_handle);
        if (d != NULL) {
            // Posted for a stored-key encryption too, not only for a pairing.
            EVT(d, "security done status=0x%x", event->pairing_complete.status);
            log_security(d, "after security");
            // Not the point to accept a new pairing: NimBLE posts this
            // before it stores the keys (302af13 report: "no stored peer
            // keys" right here, and accepting ended pairing mode so that
            // the keys stored a moment later read as a replacement).
        }
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        d = dev_by_handle(event->enc_change.conn_handle);
        if (d == NULL) {
            return 0;
        }
        d->sec_pending = false; // decided either way; never start it again on this link
        if (event->enc_change.status != 0 && (!peers_only || approved_for(&d->addr)) && !d->repaired) {
            // Pairing mode, and the stored key did not work: the device says
            // it has none (it was re-paired elsewhere or reset), or it did
            // not answer. Replacing our bond is a new pairing, so only in
            // pairing mode; otherwise anything claiming a bonded address
            // could pair itself in. (a0d8f9e report: a stale key that the
            // device ignored used to leave Forget all devices as the only
            // way out.)
            if (!is_bonded(&d->addr)) {
                // We hold no key, so this was a fresh pairing request that
                // the device refused (0x505): it still holds a key for us
                // from before and expects encryption instead. Only clearing
                // Orbit on the device helps; reconnecting at once looped
                // every 0.5 s (e72c27a report: 9 and 7 rounds).
                give_up_on(d, "device refused to pair: it still holds an old key for Orbit; clear Orbit on the device",
                           hci(event->enc_change.status));
                return 0;
            }
            EVT(d, "stored key failed status=0x%x, pairing again (%s)", event->enc_change.status,
                peers_only ? "approved" : "pairing mode");
            d->repaired = true;
            ble_store_util_delete_peer(&d->addr);
            int rc = ble_gap_security_initiate(d->conn_handle);
            if (rc != 0) {
                // The failed procedure may still occupy NimBLE's one SM slot
                // (rc=0x6); retry from periodic_check() instead of dropping
                // the link.
                EVT(d, "pairing start failed rc=0x%x, retrying", rc);
                d->sec_pending = true;
                d->encrypted = false;
            }
            return 0;
        }
        if (event->enc_change.status == BLE_HS_HCI_ERR(BLE_ERR_PINKEY_MISSING) && peers_only) {
            ask_approval(d, "device has no key for us");
            give_up_on(d, "device lost the bond; press the button (or Pair new device) to pair it again", 0);
            return 0;
        }
        if (event->enc_change.status != 0) {
            EVT(d, "encryption failed status=0x%x", event->enc_change.status);
            log_security(d, "at failure");
            if (peers_only) {
                ask_approval(d, "stored key failed");
            }
            // Usually the device still holds keys from an earlier pairing that
            // we no longer have. Keeping the link would only block a slot
            // (6254d16 report); drop it, and the device can pair afresh.
            give_up_on(d, "encryption failed", hci(event->enc_change.status));
            return 0;
        }
        if (d->encrypted) {
            // A second event on an accepted link: a key refresh, or the
            // end of a pairing that ran on after we accepted the link.
            uint32_t now_tag = key_tag(&d->addr);
            if (d->key_tag == 0 && now_tag != 0) {
                // The link was accepted before its keys were stored (a new
                // pairing); this is the first key on it, not a replacement.
                EVT(d, "keys stored for this new pairing");
                d->key_tag = now_tag;
                return 0;
            }
            if (now_tag != d->key_tag && peers_only && !approved_for(&d->addr)) {
                EVT(d, "WARNING: key replaced outside pairing mode");
                ask_approval(d, "device paired with a new key");
                give_up_on(d, "key replaced outside pairing mode", 0);
                return 0;
            }
            if (now_tag != d->key_tag && peers_only) {
                EVT(d, "new key accepted (approved by the user)");
                approved.until_us = 0;
                *refusals_of(&d->addr) = 0;
            }
            EVT(d, "encryption changed again (%s)", now_tag == d->key_tag ? "same key" : "new key, pairing mode");
            d->key_tag = now_tag;
            return 0;
        }
        on_encrypted(d, "");
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // The device wants to pair although we hold a bond for it. Replacing
        // the bond is only allowed in pairing mode (see PINKEY_MISSING above).
        d = dev_by_handle(event->repeat_pairing.conn_handle);
        if (peers_only && (d == NULL || !approved_for(&d->addr))) {
            if (d != NULL) {
                ask_approval(d, "device asked to pair again");
                give_up_on(d, "device asked to pair again; press the button (or Pair new device) to allow it", 0);
            }
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            struct ble_sm_io io = { .action = BLE_SM_IOACT_NUMCMP, .numcmp_accept = 1 };
            ble_sm_inject_io(event->passkey.conn_handle, &io);
        } else {
            ESP_LOGW(TAG, "passkey action %d not supported", event->passkey.params.action);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        d = dev_by_handle(event->mtu.conn_handle);
        if (d != NULL) {
            EVT(d, "mtu=%u", event->mtu.value);
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        d = dev_by_handle(event->notify_rx.conn_handle);
        if (d != NULL && !event->notify_rx.indication) {
            on_notify(d, event->notify_rx.attr_handle, event->notify_rx.om);
        }
        return 0;

    default:
        return 0;
    }
}

// ---- host lifecycle ----

static void on_sync(void) {
    // Also reached after a host reset: every link is gone, tell the core.
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].connected) {
            orbit_disconnect_t item = { .slot = (uint8_t) i };
            xQueueSend(disconnect_q, &item, 0);
        }
        portENTER_CRITICAL(&stats_mux);
        devs[i].connected = false;
        devs[i].in_use = false;
        portEXIT_CRITICAL(&stats_mux);
    }
    wake();
    connecting = false;
    wl_waiting = false;
    scanning = false;
    hold_until_us = 0;
    fast_until_us = esp_timer_get_time() + FAST_WAIT_US;
    wait_since_us = 0;
    wait_logged_n = -1;

    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0;
    ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS);
    peers_only = n > 0; // nothing bonded yet: accept any HID device, as upstream
    pairing_since_us = peers_only ? 0 : esp_timer_get_time();
    olog("M1 EVT t=%.3f ble ready bonds=%d\n", now_s(), n);
    for (int i = 0; i < n; i++) {
        olog("M1 EVT t=%.3f bond %d addr=..:%02x:%02x kind=%s\n", now_s(), i + 1, peers[i].val[1], peers[i].val[0],
             addr_kind(&peers[i]));
    }
    orbit_ledger_sync_bonds(peers, n);
    orbit_ledger_row_t row;
    for (int i = 0; orbit_ledger_nth(i, &row); i++) {
        char name[64];
        orbit_ledger_display_name(&row, name, sizeof(name));
        olog("M1 LDG t=%.3f port=%d addr=..:%02x:%02x %s key=%s kind=%s vid=%04x pid=%04x hash=%08x last=%lu \"%s\"\n",
             now_s(), row.port, row.addr.val[1], row.addr.val[0], addr_kind(&row.addr),
             (row.flags & ORBIT_LEDGER_NO_KEY) ? "none" : "yes", orbit_ledger_kind_name(row.kind), row.vid, row.pid,
             (unsigned) row.map_hash, (unsigned long) row.last_used, name);
    }
    schedule();
}

static void on_reset(int reason) {
    olog("M1 EVT t=%.3f ble host reset reason=%d\n", now_s(), reason);
    scanning = false;
    connecting = false;
    wl_waiting = false;
}

static void host_task(void* param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void orbit_ble_start(TaskHandle_t wake) {
    wake_task = wake;
    report_q = xQueueCreate(REPORT_QUEUE_LEN, sizeof(orbit_report_t));
    report_map_q = xQueueCreate(ORBIT_MAX_DEVS, sizeof(orbit_report_map_t));
    disconnect_q = xQueueCreate(ORBIT_MAX_DEVS * 2, sizeof(orbit_disconnect_t));

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Answers the device's own GATT requests (device name, service list, MTU).
    // Without a GATT server NimBLE drops them unanswered and the MD600
    // disconnects after 30 s (q31-results.md §3).
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("Orbit");

    ble_store_config_init();
    ble_npl_event_init(&periodic_ev, periodic_check, NULL);
    ble_npl_event_init(&pair_ev, pair_new_device_ev, NULL);
    ble_npl_event_init(&stop_pair_ev, stop_pairing_ev, NULL);
    ble_npl_event_init(&clear_bonds_ev, clear_bonds_on_host, NULL);
    ble_npl_event_init(&cmd_ev, run_commands, NULL);
    ble_npl_event_init(&approve_ev, approve_ev_fn, NULL);
    cmd_q = xQueueCreate(8, sizeof(cmd_t));
    ble_npl_callout_init(&resume_co, nimble_port_get_dflt_eventq(), resume, NULL);
    nimble_port_freertos_init(host_task);
}

bool orbit_ble_take_report(orbit_report_t* out) {
    return xQueueReceive(report_q, out, 0) == pdTRUE;
}

bool orbit_ble_take_report_map(orbit_report_map_t* out) {
    return xQueueReceive(report_map_q, out, 0) == pdTRUE;
}

bool orbit_ble_take_disconnect(orbit_disconnect_t* out) {
    return xQueueReceive(disconnect_q, out, 0) == pdTRUE;
}

uint32_t orbit_ble_take_lost(void) {
    portENTER_CRITICAL(&stats_mux);
    uint32_t n = reports_lost;
    reports_lost = 0;
    portEXIT_CRITICAL(&stats_mux);
    return n;
}

// These touch host state, so they run on the NimBLE host task.
void orbit_ble_poll(void) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &periodic_ev);
}

void orbit_ble_pair_new_device(void) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &pair_ev);
}

void orbit_ble_clear_bonds(void) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &clear_bonds_ev);
}

void orbit_ble_stop_pairing(void) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &stop_pair_ev);
}

static void post_command(cmd_t c) {
    if (xQueueSend(cmd_q, &c, 0) == pdTRUE) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &cmd_ev);
    }
}

void orbit_ble_forget(int port) {
    post_command((cmd_t) { .kind = CMD_FORGET, .a = port });
}

void orbit_ble_move(int new_port, int old_port) {
    post_command((cmd_t) { .kind = CMD_MOVE, .a = new_port, .b = old_port });
}

void orbit_ble_approve(void) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &approve_ev);
}

void orbit_ble_approval(orbit_approval_t* out) {
    int64_t now = esp_timer_get_time();
    memset(out, 0, sizeof(*out));
    out->wanted = approval.wanted;
    if (approval.wanted) {
        out->port = approval.port;
        out->reason = approval.reason;
        int64_t left = APPROVAL_PROMPT_US - (now - approval.since_us);
        out->remaining_s = left > 0 ? (int) (left / 1000000) : 0;
    }
    if (approved.until_us > now) {
        out->granted_port = orbit_ledger_port(&approved.addr);
        out->granted_remaining_s = (int) ((approved.until_us - now) / 1000000);
    }
}

int orbit_ble_slot_port(int slot) {
    return devs[slot].connected ? orbit_ledger_port(&devs[slot].addr) : 0;
}

bool orbit_ble_duplicates(void) {
    return duplicates_seen;
}

bool orbit_ble_bonds_full(void) {
    return bonds_full_seen;
}

int orbit_ble_pairing_remaining_s(void) {
    if (peers_only) {
        return 0;
    }
    if (pairing_since_us == 0 || orbit_ledger_count() == 0) {
        return -1; // no end
    }
    int64_t left = PAIRING_TIMEOUT_US - (esp_timer_get_time() - pairing_since_us);
    return left > 0 ? (int) (left / 1000000) : 0;
}

void orbit_ble_take_stats(int i, orbit_dev_stats_t* out) {
    dev_t* d = &devs[i];
    portENTER_CRITICAL(&stats_mux);
    out->connected = d->connected;
    out->conn_handle = d->conn_handle;
    out->addr_lo[0] = d->addr.val[0];
    out->addr_lo[1] = d->addr.val[1];
    out->encrypted = d->encrypted;
    out->subscribed = d->subscribed;
    out->reports = d->reports;
    out->max_gap_us = d->max_gap_us;
    memcpy(out->gaps, d->gaps, sizeof(out->gaps));
    out->total_reports = d->total_reports;
    out->disconnects = d->disconnects;
    d->reports = 0;
    d->max_gap_us = 0;
    memset(d->gaps, 0, sizeof(d->gaps));
    portEXIT_CRITICAL(&stats_mux);
    out->port = out->connected ? orbit_ledger_port(&d->addr) : 0;
}

int orbit_ble_connected_count(void) {
    int n = 0;
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        n += devs[i].connected;
    }
    return n;
}

bool orbit_ble_scanning(void) {
    return scanning;
}

bool orbit_ble_waiting(void) {
    return wl_waiting;
}

bool orbit_ble_pairing(void) {
    return !peers_only;
}

void orbit_ble_last_event(char* buf, int len) {
    portENTER_CRITICAL(&stats_mux);
    snprintf(buf, len, "%s", last_event);
    portEXIT_CRITICAL(&stats_mux);
}
