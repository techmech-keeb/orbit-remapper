// BLE central for the Q31 experiment: connects up to two HID-over-GATT
// devices, turns on their input reports and counts the reports that arrive.
// Report contents are not parsed.

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "ble_central.h"

#define UUID_HID_SERVICE 0x1812
#define UUID_REPORT      0x2A4D
#define UUID_CCCD        0x2902

#define MAX_HID_SVCS 2
#define MAX_CHRS     32
#define CONNECT_TIMEOUT_MS 10000
#define ENC_WAIT_US (5 * 1000000)

#if CONFIG_Q31_PEER_UPDATE_REJECT
#define ACCEPT_PEER_UPDATE 0
#else
#define ACCEPT_PEER_UPDATE 1
#endif

// Provided by NimBLE's NVS-backed store; it has no public header.
void ble_store_config_init(void);

typedef struct {
    uint16_t def_handle;
    uint16_t val_handle;
    uint16_t end_handle;
    uint16_t uuid16;
    uint8_t props;
} chr_t;

typedef struct {
    // Connection state; slot is in use when connecting or connected.
    bool in_use;
    bool connected;
    uint16_t conn_handle;
    ble_addr_t addr;
    bool encrypted;
    bool repaired; // already retried pairing after the device lost our bond
    int64_t connected_us;

    bool discovering;
    bool discovery_finished;
    bool rediscover; // encryption came up while discovering unencrypted
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
    uint16_t cccd_handle;
    int subscribed;

    // Report statistics
    int64_t last_report_us;
    uint32_t reports;
    uint32_t max_gap_us;
    uint32_t gaps[GAP_BUCKETS];
    uint32_t total_reports;
    uint32_t disconnects;
} dev_t;

static const char *TAG = "q31";
static dev_t devs[Q31_MAX_DEVS];
static portMUX_TYPE stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t own_addr_type;
static bool clear_bonds_on_sync;
static bool connecting;
static volatile bool scanning;
static int last_scan_rc;
static struct ble_npl_event periodic_ev;
static char last_event[24] = "START";

static int gap_event(struct ble_gap_event *event, void *arg);
static void discover_next_chr_dscs(dev_t *d);

static double ms(uint16_t units_1250us)
{
    return units_1250us * 1.25;
}

static int hci(int status)
{
    return status >= BLE_HS_ERR_HCI_BASE ? status - BLE_HS_ERR_HCI_BASE : -1;
}

static void set_last_event(const char *what, int slot, int status)
{
    portENTER_CRITICAL(&stats_mux);
    if (status < 0) {
        snprintf(last_event, sizeof(last_event), "%s D%d", what, slot);
    } else {
        snprintf(last_event, sizeof(last_event), "%s D%d %X", what, slot, status);
    }
    portEXIT_CRITICAL(&stats_mux);
}

// Event lines share one prefix so they can be grepped out of a long log.
#define EVT(d, fmt, ...)                                                                   \
    printf("Q31 EVT t=%.3f D%d addr=..:%02x:%02x " fmt "\n", esp_timer_get_time() / 1e6, \
           (int)((d) - devs), (d)->addr.val[1], (d)->addr.val[0], ##__VA_ARGS__)

static dev_t *dev_by_handle(uint16_t conn_handle)
{
    for (int i = 0; i < Q31_MAX_DEVS; i++) {
        if (devs[i].connected && devs[i].conn_handle == conn_handle) {
            return &devs[i];
        }
    }
    return NULL;
}

static bool addr_in_use(const ble_addr_t *addr)
{
    for (int i = 0; i < Q31_MAX_DEVS; i++) {
        if (devs[i].in_use && ble_addr_cmp(&devs[i].addr, addr) == 0) {
            return true;
        }
    }
    return false;
}

static dev_t *free_slot(void)
{
    for (int i = 0; i < Q31_MAX_DEVS; i++) {
        if (!devs[i].in_use) {
            return &devs[i];
        }
    }
    return NULL;
}

static void print_params(dev_t *d, const char *what)
{
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(d->conn_handle, &desc) == 0) {
        EVT(d, "%s itvl=%u(%.2fms) lat=%u to=%u(%ums)", what, desc.conn_itvl, ms(desc.conn_itvl),
            desc.conn_latency, desc.supervision_timeout, desc.supervision_timeout * 10);
    }
}

static void start_scan(void)
{
    if (connecting || scanning || free_slot() == NULL) {
        return;
    }
    // Active scan: some devices only put the HID UUID in the scan response.
    const struct ble_gap_disc_params params = {
        .itvl = 0x60,   // 60 ms
        .window = 0x30, // 30 ms
        .filter_duplicates = 1,
    };
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc == 0) {
        scanning = true;
        printf("Q31 EVT t=%.3f scan start\n", esp_timer_get_time() / 1e6);
    } else if (rc != last_scan_rc) {
        // Retried every second by periodic_check(), so only report changes.
        printf("Q31 EVT t=%.3f scan start failed rc=0x%x hci=0x%02x\n", esp_timer_get_time() / 1e6, rc, hci(rc));
    }
    last_scan_rc = rc;
}

static void start_discovery(dev_t *d);

static void periodic_check(struct ble_npl_event *ev)
{
    // Some devices never finish encryption; subscribe anyway so the log
    // shows whether they need it (the CCCD write then fails).
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < Q31_MAX_DEVS; i++) {
        dev_t *d = &devs[i];
        if (d->connected && !d->encrypted && !d->discovering && now - d->connected_us > ENC_WAIT_US) {
            EVT(d, "no encryption after %d s, discovering anyway", ENC_WAIT_US / 1000000);
            start_discovery(d);
        }
    }
    start_scan();
}

static bool is_bonded(const ble_addr_t *addr)
{
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int n = 0;
    if (ble_store_util_bonded_peers(peers, &n, CONFIG_BT_NIMBLE_MAX_BONDS) != 0) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (ble_addr_cmp(&peers[i], addr) == 0) {
            return true;
        }
    }
    return false;
}

// A keyboard waking from sleep may advertise without the HID UUID
// (prior-art.md, esp32-hid-gamepad-bridge §4.20), so bonded addresses and
// directed advertising count as well.
static bool is_candidate(const struct ble_gap_disc_desc *disc)
{
    if (disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND || is_bonded(&disc->addr)) {
        return true;
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

static void connect_to(const struct ble_gap_disc_desc *disc)
{
    dev_t *d = free_slot();
    if (d == NULL || connecting) {
        return;
    }
    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "scan cancel failed rc=%d", rc);
        return;
    }
    scanning = false;

    portENTER_CRITICAL(&stats_mux);
    memset(d, 0, offsetof(dev_t, total_reports)); // keep the slot's running totals
    d->addr = disc->addr;
    portEXIT_CRITICAL(&stats_mux);

#if CONFIG_Q31_ITVL_AT_CONNECT
    const struct ble_gap_conn_params params = {
        .scan_itvl = 0x10,
        .scan_window = 0x10,
        .itvl_min = CONFIG_Q31_ITVL_UNITS,
        .itvl_max = CONFIG_Q31_ITVL_UNITS,
        .latency = 0,
        .supervision_timeout = CONFIG_Q31_SUPERVISION_TIMEOUT,
        .min_ce_len = CONFIG_Q31_CE_LEN,
        .max_ce_len = CONFIG_Q31_CE_LEN,
    };
    const struct ble_gap_conn_params *p = &params;
#else
    const struct ble_gap_conn_params *p = NULL; // NimBLE defaults, as esp_hidh does
#endif
    rc = ble_gap_connect(own_addr_type, &disc->addr, CONNECT_TIMEOUT_MS, p, gap_event, NULL);
    if (rc != 0) {
        EVT(d, "connect start failed rc=0x%x hci=0x%02x", rc, hci(rc));
        start_scan();
        return;
    }
    d->in_use = true;
    connecting = true;
    EVT(d, "connecting rssi=%d adv_type=%u", disc->rssi, disc->event_type);
}

static dev_t *connecting_slot(void)
{
    for (int i = 0; i < Q31_MAX_DEVS; i++) {
        if (devs[i].in_use && !devs[i].connected) {
            return &devs[i];
        }
    }
    return NULL;
}

// ---- GATT: find every Report characteristic and write 0x0001 to its CCCD ----

static void restart_discovery_if_unsubscribed(dev_t *d)
{
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

static void discovery_done(dev_t *d)
{
    d->discovery_finished = true;
    EVT(d, "subscribed %d report(s)", d->subscribed);
    if (d->rediscover) {
        restart_discovery_if_unsubscribed(d);
    }
}

static int on_cccd_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr, void *arg)
{
    dev_t *d = arg;
    if (!d->connected || d->conn_handle != conn_handle) {
        return 0;
    }
    if (error->status == 0) {
        d->subscribed++;
    } else {
        EVT(d, "cccd write failed handle=%u status=0x%x", d->cccd_handle, error->status);
    }
    d->cur_chr++;
    discover_next_chr_dscs(d);
    return 0;
}

static int on_dsc(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                  const struct ble_gatt_dsc *dsc, void *arg)
{
    dev_t *d = arg;
    if (!d->connected || d->conn_handle != conn_handle) {
        return 0;
    }
    if (error->status == 0) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(UUID_CCCD)) == 0) {
            d->cccd_handle = dsc->handle;
        }
        return 0;
    }
    if (error->status == BLE_HS_EDONE && d->cccd_handle != 0) {
        static const uint8_t notify_on[2] = {0x01, 0x00};
        int rc = ble_gattc_write_flat(conn_handle, d->cccd_handle, notify_on, sizeof(notify_on),
                                      on_cccd_written, d);
        if (rc == 0) {
            return 0;
        }
        EVT(d, "cccd write start failed rc=0x%x", rc);
    } else if (error->status != BLE_HS_EDONE) {
        EVT(d, "descriptor discovery failed status=0x%x", error->status);
    }
    d->cur_chr++;
    discover_next_chr_dscs(d);
    return 0;
}

static void discover_next_chr_dscs(dev_t *d)
{
    for (; d->cur_chr < d->n_chrs; d->cur_chr++) {
        const chr_t *c = &d->chrs[d->cur_chr];
        if (c->uuid16 != UUID_REPORT || !(c->props & BLE_GATT_CHR_PROP_NOTIFY) ||
            c->end_handle <= c->val_handle) {
            continue; // output/feature reports have no notifications
        }
        d->cccd_handle = 0;
        int rc = ble_gattc_disc_all_dscs(d->conn_handle, c->val_handle, c->end_handle, on_dsc, d);
        if (rc == 0) {
            return;
        }
        EVT(d, "descriptor discovery start failed rc=0x%x", rc);
    }
    discovery_done(d);
}

static void discover_next_svc_chrs(dev_t *d);

static int on_chr(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                  void *arg)
{
    dev_t *d = arg;
    if (!d->connected || d->conn_handle != conn_handle) {
        return 0;
    }
    if (error->status == 0) {
        if (d->n_chrs < MAX_CHRS) {
            chr_t *c = &d->chrs[d->n_chrs++];
            c->def_handle = chr->def_handle;
            c->val_handle = chr->val_handle;
            c->end_handle = d->svc_end[d->cur_svc];
            c->props = chr->properties;
            c->uuid16 = chr->uuid.u.type == BLE_UUID_TYPE_16 ? ble_uuid_u16(&chr->uuid.u) : 0;
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

static void discover_next_svc_chrs(dev_t *d)
{
    if (d->cur_svc < d->n_svcs) {
        int rc = ble_gattc_disc_all_chrs(d->conn_handle, d->svc_start[d->cur_svc], d->svc_end[d->cur_svc],
                                         on_chr, d);
        if (rc == 0) {
            return;
        }
        EVT(d, "characteristic discovery start failed rc=0x%x", rc);
    }
    d->cur_chr = 0;
    discover_next_chr_dscs(d);
}

static int on_svc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                  void *arg)
{
    dev_t *d = arg;
    if (!d->connected || d->conn_handle != conn_handle) {
        return 0;
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
    discover_next_svc_chrs(d);
    return 0;
}

static void start_discovery(dev_t *d)
{
    if (d->discovering) {
        return;
    }
    d->discovering = true;
    int rc = ble_gattc_disc_svc_by_uuid(d->conn_handle, BLE_UUID16_DECLARE(UUID_HID_SERVICE), on_svc, d);
    if (rc != 0) {
        EVT(d, "service discovery start failed rc=0x%x", rc);
    }
}

// ---- GAP events ----

static void on_report(dev_t *d)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&stats_mux);
    if (d->last_report_us != 0) {
        int64_t gap = now - d->last_report_us;
        if (gap < 1000000) {
            if (gap > d->max_gap_us) {
                d->max_gap_us = (uint32_t)gap;
            }
            d->gaps[gap <= 8000 ? GAP_LE_8MS : gap <= 16000 ? GAP_LE_16MS : gap <= 32000 ? GAP_LE_32MS : GAP_OVER_32MS]++;
        }
    }
    d->last_report_us = now;
    d->reports++;
    d->total_reports++;
    portEXIT_CRITICAL(&stats_mux);
}

static int on_update_request(dev_t *d, const char *kind, const struct ble_gap_upd_params *peer)
{
    d->peer_allows_itvl = peer->itvl_min <= CONFIG_Q31_ITVL_UNITS && CONFIG_Q31_ITVL_UNITS <= peer->itvl_max;
    EVT(d, "%s from device itvl=%u-%u(%.2f-%.2fms) lat=%u to=%u -> %s", kind, peer->itvl_min, peer->itvl_max,
        ms(peer->itvl_min), ms(peer->itvl_max), peer->latency, peer->supervision_timeout,
        ACCEPT_PEER_UPDATE ? "accept" : "reject");
    set_last_event("UPDREQ", (int)(d - devs), peer->itvl_min);
    return ACCEPT_PEER_UPDATE ? 0 : BLE_ERR_CONN_PARMS;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    dev_t *d;

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
            set_last_event("CONNFAIL", (int)(d - devs), event->connect.status);
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
        set_last_event("CONN", (int)(d - devs), -1);
        // HOGP devices may not send reports until the link is encrypted
        // (prior-art.md §4.29), so start it ourselves.
        int rc = ble_gap_security_initiate(d->conn_handle);
        if (rc != 0) {
            EVT(d, "security start failed rc=0x%x, discovering anyway", rc);
            start_discovery(d);
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
        set_last_event("DISC", (int)(d - devs), hci(event->disconnect.reason));
        portENTER_CRITICAL(&stats_mux);
        d->connected = false;
        d->in_use = false;
        d->disconnects++;
        portEXIT_CRITICAL(&stats_mux);
        start_scan(); // reconnect (brief 3-9)
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        d = dev_by_handle(event->conn_update.conn_handle);
        if (d == NULL) {
            return 0;
        }
        if (event->conn_update.status != 0) {
            EVT(d, "update failed status=0x%x hci=0x%02x", event->conn_update.status,
                hci(event->conn_update.status));
            set_last_event("UPDFAIL", (int)(d - devs), hci(event->conn_update.status));
        } else {
            print_params(d, "updated");
            set_last_event("UPD", (int)(d - devs), -1);
#if CONFIG_Q31_PEER_UPDATE_KEEP_ITVL
            struct ble_gap_conn_desc desc;
            if (d->peer_allows_itvl && !d->reasserted && ble_gap_conn_find(d->conn_handle, &desc) == 0 &&
                desc.conn_itvl != CONFIG_Q31_ITVL_UNITS) {
                d->reasserted = true;
                const struct ble_gap_upd_params upd = {
                    .itvl_min = CONFIG_Q31_ITVL_UNITS,
                    .itvl_max = CONFIG_Q31_ITVL_UNITS,
                    .latency = desc.conn_latency,
                    .supervision_timeout = desc.supervision_timeout,
                    .min_ce_len = CONFIG_Q31_CE_LEN,
                    .max_ce_len = CONFIG_Q31_CE_LEN,
                };
                int rc = ble_gap_update_params(d->conn_handle, &upd);
                EVT(d, "device allows %.2fms, asking for it again rc=0x%x", ms(CONFIG_Q31_ITVL_UNITS), rc);
            }
#endif
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
            // The device forgot our bond (e.g. it was re-paired elsewhere): drop ours and pair again.
            EVT(d, "device lost the bond, pairing again");
            d->repaired = true;
            ble_store_util_delete_peer(&d->addr);
            ble_gap_security_initiate(d->conn_handle);
            return 0;
        }
        d->encrypted = event->enc_change.status == 0;
        EVT(d, "encryption %s status=0x%x", d->encrypted ? "on" : "failed", event->enc_change.status);
#if CONFIG_Q31_ITVL_AFTER_CONNECT
        {
            const struct ble_gap_upd_params upd = {
                .itvl_min = CONFIG_Q31_ITVL_UNITS,
                .itvl_max = CONFIG_Q31_ITVL_UNITS,
                .latency = 0,
                .supervision_timeout = CONFIG_Q31_SUPERVISION_TIMEOUT,
                .min_ce_len = CONFIG_Q31_CE_LEN,
                .max_ce_len = CONFIG_Q31_CE_LEN,
            };
            int urc = ble_gap_update_params(d->conn_handle, &upd);
            EVT(d, "requested update to %.2fms rc=0x%x", ms(CONFIG_Q31_ITVL_UNITS), urc);
        }
#endif
        if (d->encrypted && d->discovering) {
            // Subscribing before encryption may have failed; retry once it is up.
            if (d->discovery_finished) {
                restart_discovery_if_unsubscribed(d);
            } else {
                d->rediscover = true;
            }
        }
        start_discovery(d);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // We still hold a bond the device no longer has: replace it.
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            struct ble_sm_io io = {.action = BLE_SM_IOACT_NUMCMP, .numcmp_accept = 1};
            ble_sm_inject_io(event->passkey.conn_handle, &io);
        } else {
            ESP_LOGW(TAG, "passkey action %d not supported", event->passkey.params.action);
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        d = dev_by_handle(event->notify_rx.conn_handle);
        if (d != NULL) {
            on_report(d);
        }
        return 0;

    default:
        return 0;
    }
}

// ---- host lifecycle ----

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);
    if (clear_bonds_on_sync) {
        ble_store_clear();
        printf("Q31 EVT t=%.3f bonds cleared\n", esp_timer_get_time() / 1e6);
    }
    start_scan();
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "host reset reason=%d", reason);
    scanning = false;
    connecting = false;
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_central_start(bool clear_bonds)
{
    clear_bonds_on_sync = clear_bonds;
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

    ble_store_config_init();
    ble_npl_event_init(&periodic_ev, periodic_check, NULL);
    nimble_port_freertos_init(host_task);
}

void ble_central_poll(void)
{
    // start_scan() touches host state, so run it on the NimBLE host task.
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &periodic_ev);
}

bool ble_central_connecting(void)
{
    return connecting;
}

void ble_central_take_stats(int i, q31_dev_stats_t *out)
{
    dev_t *d = &devs[i];
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

bool ble_central_scanning(void)
{
    return scanning;
}

void ble_central_last_event(char *buf, int len)
{
    portENTER_CRITICAL(&stats_mux);
    snprintf(buf, len, "%s", last_event);
    portEXIT_CRITICAL(&stats_mux);
}
