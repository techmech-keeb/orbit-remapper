// Logs the ATT, SMP and L2CAP signalling packets exchanged with each device,
// to find out what a device asks for before it drops the link.
//
// The two packet paths between the NimBLE host and the controller are
// intercepted at link time (see main/CMakeLists.txt):
//   controller -> host: ble_transport_to_hs_acl_impl()
//   host -> controller: esp_vhci_host_send_packet()
// Only opcodes and a few non-secret header bytes are kept. Addresses and key
// material never leave the packet.

#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "os/os_mbuf.h"
#include "pkt_log.h"

#define CID_ATT 0x0004
#define CID_SIG 0x0005
#define CID_SMP 0x0006

#define ATT_ERROR_RSP  0x01
#define ATT_NOTIFY     0x1B
#define ATT_INDICATE   0x1D
#define ATT_CONFIRM    0x1E

#define SMP_PAIRING_REQ    0x01
#define SMP_PAIRING_RSP    0x02
#define SMP_PAIRING_FAILED 0x05
#define SMP_SECURITY_REQ   0x0B

#define RING_SIZE 128
#define PARAM_MAX 6
#define NO_RESPONSE_US 1000000
#define PENDING_FORGET_US 40000000 // past ATT's 30 s timeout the link is gone anyway

typedef struct {
    int64_t t_us;
    uint16_t conn_handle;
    uint16_t cid;
    uint16_t len; // L2CAP payload length
    bool rx;
    uint8_t n;    // bytes kept in b[]
    uint8_t b[1 + PARAM_MAX];
} pkt_t;

static pkt_t ring[RING_SIZE];
static unsigned ring_head, ring_tail, ring_dropped;
static uint32_t notify_rx;
static portMUX_TYPE ring_mux = portMUX_INITIALIZER_UNLOCKED;

// Requests from the device that still wait for our answer (drain task only).
typedef struct {
    uint16_t conn_handle;
    uint8_t op;
    int64_t t_us;
    bool reported;
} pending_t;
static pending_t pending[8];

// How many bytes after the opcode are safe and useful to keep.
static int params_to_keep(uint16_t cid, uint8_t op, bool rx)
{
    switch (cid) {
    case CID_ATT:
        // The device's own requests say what it looks for (handles, UUIDs,
        // MTU). Our requests and its responses are already known.
        return rx && op != ATT_NOTIFY ? PARAM_MAX : (op == ATT_ERROR_RSP ? 4 : 0);
    case CID_SMP:
        // Pairing Request/Response carry IO capability, auth flags, key size
        // and key distribution; Security Request the auth flags; Pairing
        // Failed the reason. Every other SMP packet carries key material.
        return op == SMP_PAIRING_REQ || op == SMP_PAIRING_RSP ? 6
             : op == SMP_PAIRING_FAILED || op == SMP_SECURITY_REQ ? 1 : 0;
    case CID_SIG:
        return 1; // identifier
    default:
        return 0;
    }
}

// hci points at an HCI ACL packet (handle/flags, length, then L2CAP).
static void capture(const uint8_t *hci, int len, bool rx)
{
    if (len < 9) {
        return;
    }
    uint16_t handle_flags = hci[0] | hci[1] << 8;
    if (((handle_flags >> 12) & 0x3) == 0x1) {
        return; // continuation fragment: no L2CAP header
    }
    uint16_t cid = hci[6] | hci[7] << 8;
    if (cid != CID_ATT && cid != CID_SMP && cid != CID_SIG) {
        return;
    }
    uint8_t op = hci[8];
    if (rx && cid == CID_ATT && op == ATT_NOTIFY) {
        portENTER_CRITICAL_SAFE(&ring_mux);
        notify_rx++;
        portEXIT_CRITICAL_SAFE(&ring_mux);
        return; // input reports; counted per device elsewhere
    }

    pkt_t p = {
        .t_us = esp_timer_get_time(),
        .conn_handle = handle_flags & 0x0FFF,
        .cid = cid,
        .len = hci[4] | hci[5] << 8,
        .rx = rx,
    };
    int keep = params_to_keep(cid, op, rx);
    if (keep > len - 9) {
        keep = len - 9;
    }
    p.b[0] = op;
    memcpy(&p.b[1], &hci[9], keep);
    p.n = 1 + keep;

    portENTER_CRITICAL_SAFE(&ring_mux);
    if (ring_head - ring_tail < RING_SIZE) {
        ring[ring_head++ % RING_SIZE] = p;
    } else {
        ring_dropped++;
    }
    portEXIT_CRITICAL_SAFE(&ring_mux);
}

int __real_ble_transport_to_hs_acl_impl(struct os_mbuf *om);
int __wrap_ble_transport_to_hs_acl_impl(struct os_mbuf *om)
{
    uint8_t head[9 + PARAM_MAX];
    int len = OS_MBUF_PKTLEN(om);
    if (len > (int)sizeof(head)) {
        len = sizeof(head);
    }
    if (os_mbuf_copydata(om, 0, len, head) == 0) {
        capture(head, len, true);
    }
    return __real_ble_transport_to_hs_acl_impl(om);
}

void __real_esp_vhci_host_send_packet(uint8_t *data, uint16_t len);
void __wrap_esp_vhci_host_send_packet(uint8_t *data, uint16_t len)
{
    if (len > 1 && data[0] == 0x02) { // H4 packet type: ACL data
        capture(data + 1, len - 1, false);
    }
    __real_esp_vhci_host_send_packet(data, len);
}

// ---- printing (report core, never on the BLE path) ----

static const char *att_name(uint8_t op)
{
    switch (op) {
    case 0x01: return "ERROR_RSP";
    case 0x02: return "MTU_REQ";
    case 0x03: return "MTU_RSP";
    case 0x04: return "FIND_INFO_REQ";
    case 0x05: return "FIND_INFO_RSP";
    case 0x06: return "FIND_BY_TYPE_VALUE_REQ";
    case 0x07: return "FIND_BY_TYPE_VALUE_RSP";
    case 0x08: return "READ_BY_TYPE_REQ";
    case 0x09: return "READ_BY_TYPE_RSP";
    case 0x0A: return "READ_REQ";
    case 0x0B: return "READ_RSP";
    case 0x0C: return "READ_BLOB_REQ";
    case 0x0D: return "READ_BLOB_RSP";
    case 0x0E: return "READ_MULTIPLE_REQ";
    case 0x0F: return "READ_MULTIPLE_RSP";
    case 0x10: return "READ_BY_GROUP_TYPE_REQ";
    case 0x11: return "READ_BY_GROUP_TYPE_RSP";
    case 0x12: return "WRITE_REQ";
    case 0x13: return "WRITE_RSP";
    case 0x16: return "PREPARE_WRITE_REQ";
    case 0x17: return "PREPARE_WRITE_RSP";
    case 0x18: return "EXECUTE_WRITE_REQ";
    case 0x19: return "EXECUTE_WRITE_RSP";
    case 0x1D: return "INDICATE";
    case 0x1E: return "CONFIRM";
    case 0x20: return "READ_MULTIPLE_VAR_REQ";
    case 0x21: return "READ_MULTIPLE_VAR_RSP";
    case 0x52: return "WRITE_CMD";
    default:   return "?";
    }
}

static const char *smp_name(uint8_t op)
{
    static const char *names[] = {
        "?", "PAIRING_REQ", "PAIRING_RSP", "PAIRING_CONFIRM", "PAIRING_RANDOM", "PAIRING_FAILED",
        "ENCRYPTION_INFO", "CENTRAL_IDENT", "IDENTITY_INFO", "IDENTITY_ADDR_INFO", "SIGNING_INFO",
        "SECURITY_REQ", "PUBLIC_KEY", "DHKEY_CHECK", "KEYPRESS",
    };
    return op < sizeof(names) / sizeof(names[0]) ? names[op] : "?";
}

static const char *sig_name(uint8_t code)
{
    switch (code) {
    case 0x01: return "COMMAND_REJECT";
    case 0x06: return "DISCONNECTION_REQ";
    case 0x07: return "DISCONNECTION_RSP";
    case 0x0A: return "INFORMATION_REQ";
    case 0x0B: return "INFORMATION_RSP";
    case 0x12: return "CONN_PARAM_UPDATE_REQ";
    case 0x13: return "CONN_PARAM_UPDATE_RSP";
    case 0x14: return "LE_CREDIT_CONN_REQ";
    case 0x15: return "LE_CREDIT_CONN_RSP";
    default:   return "?";
    }
}

// Requests from the device that must be answered (ATT, Core Vol 3 Part F 3.4).
static bool att_needs_answer(uint8_t op)
{
    return op == ATT_INDICATE || (op <= 0x20 && op >= 0x02 && (op & 1) == 0 && op != 0x14 && op != 0x1A &&
                                  op != 0x1C && op != 0x1E);
}

static bool att_answers(uint8_t sent, uint8_t req, const pkt_t *p)
{
    if (req == ATT_INDICATE) {
        return sent == ATT_CONFIRM;
    }
    return sent == req + 1 || (sent == ATT_ERROR_RSP && p->n >= 2 && p->b[1] == req);
}

static void track(const pkt_t *p)
{
    uint8_t op = p->b[0];
    if (p->rx && att_needs_answer(op)) {
        for (int i = 0; i < 8; i++) {
            if (pending[i].op == 0) {
                pending[i] = (pending_t){.conn_handle = p->conn_handle, .op = op, .t_us = p->t_us};
                return;
            }
        }
        return;
    }
    if (!p->rx) {
        for (int i = 0; i < 8; i++) {
            if (pending[i].op != 0 && pending[i].conn_handle == p->conn_handle && att_answers(op, pending[i].op, p)) {
                printf("Q31 PKT t=%.3f h=%u answered %s after %.1fms\n", p->t_us / 1e6, p->conn_handle,
                       att_name(pending[i].op), (p->t_us - pending[i].t_us) / 1000.0);
                pending[i].op = 0;
                return;
            }
        }
    }
}

static void print_pkt(const pkt_t *p)
{
    const char *proto = p->cid == CID_ATT ? "ATT" : p->cid == CID_SMP ? "SMP" : "SIG";
    const char *name = p->cid == CID_ATT ? att_name(p->b[0]) : p->cid == CID_SMP ? smp_name(p->b[0]) : sig_name(p->b[0]);
    char params[3 * PARAM_MAX + 1] = "";
    for (int i = 1; i < p->n; i++) {
        snprintf(params + 3 * (i - 1), 4, i + 1 < p->n ? "%02x " : "%02x", p->b[i]);
    }
    printf("Q31 PKT t=%.3f h=%u %s %s 0x%02x %s len=%u%s%s%s\n", p->t_us / 1e6, p->conn_handle,
           p->rx ? "rx" : "tx", proto, p->b[0], name, p->len, p->n > 1 ? " [" : "", params,
           p->n > 1 ? "]" : "");
}

static void drain_once(void)
{
    static uint32_t reported_drops;
    for (;;) {
        pkt_t p;
        portENTER_CRITICAL(&ring_mux);
        bool have = ring_tail != ring_head;
        if (have) {
            p = ring[ring_tail++ % RING_SIZE];
        }
        uint32_t dropped = ring_dropped;
        portEXIT_CRITICAL(&ring_mux);
        if (dropped != reported_drops) {
            printf("Q31 PKT lost %lu packet record(s)\n", (unsigned long)(dropped - reported_drops));
            reported_drops = dropped;
        }
        if (!have) {
            break;
        }
        print_pkt(&p);
        if (p.cid == CID_ATT) {
            track(&p);
        }
    }
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < 8; i++) {
        if (pending[i].op == 0) {
            continue;
        }
        if (!pending[i].reported && now - pending[i].t_us > NO_RESPONSE_US) {
            printf("Q31 PKT t=%.3f h=%u NO ANSWER from us to the device's %s for %.1fs\n", now / 1e6,
                   pending[i].conn_handle, att_name(pending[i].op), (now - pending[i].t_us) / 1e6);
            pending[i].reported = true;
        }
        if (now - pending[i].t_us > PENDING_FORGET_US) {
            pending[i].op = 0;
        }
    }
}

static void drain_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        drain_once();
    }
}

void pkt_log_start(void)
{
    xTaskCreatePinnedToCore(drain_task, "pkt_log", 4096, NULL, 1, NULL, 1);
}
