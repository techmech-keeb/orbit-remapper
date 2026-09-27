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
#include "log.h"

#define UUID_HID_SERVICE 0x1812
#define UUID_REPORT_MAP  0x2A4B
#define UUID_REPORT      0x2A4D
#define UUID_CCCD        0x2902
#define UUID_REPORT_REF  0x2908

#define REPORT_TYPE_INPUT 1

#define MAX_HID_SVCS 2
#define MAX_CHRS     32
#define CONNECT_TIMEOUT_MS 10000
// NimBLE cancels a connect attempt after CONNECT_TIMEOUT_MS and then waits
// for the controller's "connection complete" event, which may never come
// (ble_gap.c, ble_gap_master_timer: "XXX: Set a timer to reset the
// controller..."). Seen twice when a device vanished right after
// advertising (09b3075 report): the attempt hung until RST.
#define CONNECT_STUCK_CANCEL_US (15 * 1000000)
#define CONNECT_STUCK_RESET_US  (25 * 1000000)
#define ENC_WAIT_US (5 * 1000000)

// Connection parameters requested in the connection request (q31-results.md
// §5: asking after connecting is refused for the second device).
#define CONN_ITVL   6   // 7.5 ms
#define CONN_LATENCY 0
#define CONN_TIMEOUT 400 // 4 s

#define REPORT_QUEUE_LEN 32

// A device whose encryption failed (it still holds keys from an earlier
// pairing) reconnects at once and fails again: 280 attempts in 2 minutes,
// crowding out other devices (09b3075 report). Leave it alone for a while.
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

    bool discovering;
    bool discovery_finished;
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

    // Report statistics
    int64_t last_report_us;
    uint32_t reports;
    uint32_t max_gap_us;
    uint32_t gaps[GAP_BUCKETS];
    uint32_t total_reports;
    uint32_t disconnects;
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
    return d;
}

static const char* TAG = "ble";
static dev_t devs[ORBIT_MAX_DEVS];
static portMUX_TYPE stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t own_addr_type;
static bool connecting;
static int64_t connecting_since_us;
static bool connecting_cancel_logged;
static volatile bool scanning;
static volatile bool peers_only = true; // scan for bonded devices only
static int last_scan_rc;
static struct ble_npl_event periodic_ev, pair_ev, clear_bonds_ev;
static char last_event[24] = "START";
static TaskHandle_t wake_task;
static QueueHandle_t report_q, report_map_q, disconnect_q;
static uint32_t reports_lost;

static struct {
    ble_addr_t addr;
    int64_t until_us;
} avoid[AVOID_MAX];

static void avoid_add(const ble_addr_t* addr) {
    int64_t now = esp_timer_get_time();
    int slot = 0;
    for (int i = 0; i < AVOID_MAX; i++) {
        if (ble_addr_cmp(&avoid[i].addr, addr) == 0 || avoid[i].until_us <= now) {
            slot = i;
            break;
        }
        if (avoid[i].until_us < avoid[slot].until_us) {
            slot = i; // oldest entry, if all are live
        }
    }
    avoid[slot].addr = *addr;
    avoid[slot].until_us = now + AVOID_US;
}

static bool avoided(const ble_addr_t* addr) {
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < AVOID_MAX; i++) {
        if (avoid[i].until_us > now && ble_addr_cmp(&avoid[i].addr, addr) == 0) {
            return true;
        }
    }
    return false;
}


static int gap_event(struct ble_gap_event* event, void* arg);
static void discover_next_report(dev_t* d);
static void start_discovery(dev_t* d);

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

// Drops the link and leaves the device alone for AVOID_US.
static void give_up_on(dev_t* d, const char* why, int status) {
    EVT(d, "%s, dropping the link and ignoring the device for %d s", why, AVOID_US / 1000000);
    set_last_event("ENCFAIL", (int) (d - devs), status);
    avoid_add(&d->addr);
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

static void start_scan(void) {
    if (connecting || scanning || free_slot() == NULL) {
        return;
    }
    // Active scan: some devices only put the HID UUID in the scan response.
    // No duplicate filtering: the controller would report each address once
    // per scan, so a device skipped while it was being avoided (or while it
    // was busy) would never be seen again until the scan restarted (0a0cfcf
    // report: a bonded keyboard stayed invisible for an hour).
    const struct ble_gap_disc_params params = {
        .itvl = 0x60,   // 60 ms
        .window = 0x30, // 30 ms
        .filter_duplicates = 0,
    };
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc == 0) {
        scanning = true;
        olog("M1 EVT t=%.3f scan start (%s)\n", now_s(), peers_only ? "bonded devices only" : "pairing");
    } else if (rc != last_scan_rc) {
        // Retried every second by periodic_check(), so only report changes.
        olog("M1 EVT t=%.3f scan start failed rc=0x%x hci=0x%02x\n", now_s(), rc, hci(rc));
    }
    last_scan_rc = rc;
}

static void periodic_check(struct ble_npl_event* ev) {
    int64_t now = esp_timer_get_time();

    if (connecting && now - connecting_since_us > CONNECT_STUCK_RESET_US) {
        olog("M1 EVT t=%.3f connect attempt stuck for %d s, resetting the BLE host\n", now_s(),
             (int) ((now - connecting_since_us) / 1000000));
        ble_hs_sched_reset(BLE_HS_ETIMEOUT);
        return;
    }
    if (connecting && now - connecting_since_us > CONNECT_STUCK_CANCEL_US && !connecting_cancel_logged) {
        int rc = ble_gap_conn_cancel();
        olog("M1 EVT t=%.3f connect attempt stuck for %d s, cancel rc=0x%x\n", now_s(),
             (int) ((now - connecting_since_us) / 1000000), rc);
        connecting_cancel_logged = true;
    }

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        if (d->connected && d->sec_pending) {
            int rc = ble_gap_security_initiate(d->conn_handle);
            if (rc == 0) {
                d->sec_pending = false;
                EVT(d, "security started");
            }
        }
    }

    // A device that neither encrypts nor asks to pair within ENC_WAIT_US
    // (one that lost its key but stays silent) would otherwise hold the slot
    // until the 30 s SM timeout (0a0cfcf report). HOGP needs encryption, so
    // an unencrypted link is of no use anyway.
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        dev_t* d = &devs[i];
        if (d->connected && !d->encrypted && now - d->connected_us > ENC_WAIT_US) {
            give_up_on(d, "no encryption after 5 s", 0);
        }
    }
    start_scan();
}

static void pair_new_device_ev(struct ble_npl_event* ev) {
    olog("M1 EVT t=%.3f pair_new_device\n", now_s());
    peers_only = false;
    if (scanning) {
        ble_gap_disc_cancel();
        scanning = false;
    }
    start_scan();
}

static void clear_bonds_on_host(struct ble_npl_event* ev) {
    int rc = ble_store_clear();
    olog("M1 EVT t=%.3f clear_bonds rc=0x%x\n", now_s(), rc);
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        if (devs[i].connected) {
            ble_gap_terminate(devs[i].conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }
    peers_only = false;
}

static bool is_bonded(const ble_addr_t* addr) {
    return bond_index(addr) != 0;
}

// A keyboard waking from sleep may advertise without the HID UUID
// (prior-art.md, esp32-hid-gamepad-bridge §4.20), so bonded addresses and
// directed advertising count as well.
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
    portENTER_CRITICAL(&stats_mux);
    memset(d, 0, offsetof(dev_t, total_reports)); // keep the slot's running totals
    d->gen = gen;
    d->addr = disc->addr;
    portEXIT_CRITICAL(&stats_mux);
    d->ctx = new_gatt_ctx(d);

    const struct ble_gap_conn_params params = {
        .scan_itvl = 0x10,
        .scan_window = 0x10,
        .itvl_min = CONN_ITVL,
        .itvl_max = CONN_ITVL,
        .latency = CONN_LATENCY,
        .supervision_timeout = CONN_TIMEOUT,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    rc = ble_gap_connect(own_addr_type, &disc->addr, CONNECT_TIMEOUT_MS, &params, gap_event, NULL);
    if (rc != 0) {
        EVT(d, "connect start failed rc=0x%x hci=0x%02x", rc, hci(rc));
        start_scan();
        return;
    }
    d->in_use = true;
    connecting = true;
    connecting_since_us = esp_timer_get_time();
    connecting_cancel_logged = false;
    EVT(d, "connecting rssi=%d adv_type=%u", disc->rssi, disc->event_type);
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

static void discovery_done(dev_t* d) {
    d->discovery_finished = true;
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
    }
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

static int on_dsc(uint16_t conn_handle, const struct ble_gatt_error* error, uint16_t chr_val_handle,
                  const struct ble_gatt_dsc* dsc, void* arg) {
    dev_t* d = gatt_ctx_dev(arg, conn_handle);
    if (d == NULL) {
        return 0; // a procedure of an earlier link on this slot (8061951 report, problem G)
    }
    chr_t* c = &d->chrs[d->cur_chr];
    if (error->status == 0) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(UUID_CCCD)) == 0) {
            c->cccd_handle = dsc->handle;
        } else if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(UUID_REPORT_REF)) == 0) {
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
    m->hub_port = bond_index(&d->addr);
    EVT(d, "report map %u byte(s), hub_port=%u", m->len, m->hub_port);
    if (m->len > 0 && xQueueSend(report_map_q, m, 0) == pdTRUE) {
        wake();
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
            c->uuid16 = chr->uuid.u.type == BLE_UUID_TYPE_16 ? ble_uuid_u16(&chr->uuid.u) : 0;
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
    int rc = ble_gattc_disc_svc_by_uuid(d->conn_handle, BLE_UUID16_DECLARE(UUID_HID_SERVICE), on_svc, d->ctx);
    if (rc != 0) {
        EVT(d, "service discovery start failed rc=0x%x", rc);
    }
}

// ---- GAP events ----

static void on_notify(dev_t* d, uint16_t attr_handle, struct os_mbuf* om) {
    int64_t now = esp_timer_get_time();
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
        connecting = false;
        d = connecting_slot();
        if (d == NULL) {
            return 0;
        }
        if (event->connect.status != 0) {
            EVT(d, "connect failed status=0x%x hci=0x%02x", event->connect.status, hci(event->connect.status));
            set_last_event("CONNFAIL", (int) (d - devs), event->connect.status);
            d->in_use = false;
            start_scan();
            return 0;
        }
        portENTER_CRITICAL(&stats_mux);
        d->connected = true;
        d->conn_handle = event->connect.conn_handle;
        d->connected_us = esp_timer_get_time();
        portEXIT_CRITICAL(&stats_mux);
        print_params(d, "connected");
        set_last_event("CONN", (int) (d - devs), -1);
        // HOGP devices may not send reports until the link is encrypted
        // (prior-art.md §4.29), so start it ourselves.
        int rc = ble_gap_security_initiate(d->conn_handle);
        if (rc != 0) {
            // NimBLE runs one pairing/encryption procedure at a time
            // (BLE_SM_MAX_PROCS = 1), so this fails with rc=0x6 while the
            // other device is pairing (0a0cfcf report). Not the device's
            // fault: retry from periodic_check().
            EVT(d, "security start failed rc=0x%x, retrying", rc);
            d->sec_pending = true;
        }
        start_scan();
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
        d = dev_by_handle(event->disconnect.conn.conn_handle);
        if (d == NULL) {
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
        start_scan(); // reconnect
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

    case BLE_GAP_EVENT_ENC_CHANGE:
        d = dev_by_handle(event->enc_change.conn_handle);
        if (d == NULL) {
            return 0;
        }
        if (event->enc_change.status == BLE_HS_HCI_ERR(BLE_ERR_PINKEY_MISSING) && !d->repaired) {
            // The device says it has no key for us (it was re-paired elsewhere
            // or reset). Replacing our bond is a new pairing, so only in
            // pairing mode; otherwise anything claiming a bonded address
            // could pair itself in.
            if (peers_only) {
                give_up_on(d, "device lost the bond; press Pair new device to pair it again", 0);
                return 0;
            }
            EVT(d, "device lost the bond, pairing again (pairing mode)");
            d->repaired = true;
            ble_store_util_delete_peer(&d->addr);
            ble_gap_security_initiate(d->conn_handle);
            return 0;
        }
        d->encrypted = event->enc_change.status == 0;
        EVT(d, "encryption %s status=0x%x", d->encrypted ? "on" : "failed", event->enc_change.status);
        if (!d->encrypted) {
            // Usually the device still holds keys from an earlier pairing that
            // we no longer have. Keeping the link would only block a slot
            // (6254d16 report); drop it, and the device can pair afresh.
            give_up_on(d, "encryption failed", hci(event->enc_change.status));
            return 0;
        }
        {
            peers_only = true; // as upstream: back to bonded devices only after a successful pairing
            if (d->discovering) {
                // Subscribing before encryption may have failed; retry once it is up.
                if (d->discovery_finished) {
                    restart_discovery_if_unsubscribed(d);
                } else {
                    d->rediscover = true;
                }
            }
        }
        start_discovery(d);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // The device wants to pair although we hold a bond for it. Replacing
        // the bond is only allowed in pairing mode (see PINKEY_MISSING above).
        d = dev_by_handle(event->repeat_pairing.conn_handle);
        if (peers_only) {
            if (d != NULL) {
                give_up_on(d, "device asked to pair again; press Pair new device to allow it", 0);
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
    scanning = false;

    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0;
    ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS);
    peers_only = n > 0; // nothing bonded yet: accept any HID device, as upstream
    olog("M1 EVT t=%.3f ble ready bonds=%d\n", now_s(), n);
    start_scan();
}

static void on_reset(int reason) {
    olog("M1 EVT t=%.3f ble host reset reason=%d\n", now_s(), reason);
    scanning = false;
    connecting = false;
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
    ble_npl_event_init(&clear_bonds_ev, clear_bonds_on_host, NULL);
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

bool orbit_ble_pairing(void) {
    return !peers_only;
}

void orbit_ble_last_event(char* buf, int len) {
    portENTER_CRITICAL(&stats_mux);
    snprintf(buf, len, "%s", last_event);
    portEXIT_CRITICAL(&stats_mux);
}
