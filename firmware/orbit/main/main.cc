// Orbit M1: upstream HID Remapper on the M5Dial (ESP32-S3).
// See docs/drafts/2026-09-27/m1-brief.md.
//
// The main loop mirrors main() in upstream's firmware-bluetooth/src/main.cc
// and firmware/src/main.cc (51ab8b3). It is the only task that calls the
// core (decision I2).

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "host/ble_hs.h"
#include "tusb.h"

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

#include "ble.h"
#include "descriptor_parser.h"
#include "display.h"
#include "log.h"
#include "orbit.h"
#include "storage.h"

#define PIN_POWER_HOLD 46 // keeps the M5Dial on when running from battery
#define PIN_BUTTON     42 // screen push button, low when pressed

// Decision I3: keep a buffer for LVGL (M3) allocated from the start, so the
// free-memory numbers measured in M1 already account for it.
#define LVGL_RESERVE_BYTES (48 * 1024)

static TaskHandle_t main_task;
static std::atomic<bool> tick_pending(false);
static std::atomic<const char*> download_mode_reason(nullptr);
static void* lvgl_reserve;

double orbit_now_s() {
    return esp_timer_get_time() / 1e6;
}

// M1 LAT: time from a device's notification arriving to the report that
// followed it going to the PC on HID 0. The oldest report not yet followed by
// a send is the reference; a report that produced no output (nothing mapped
// changed) is dropped from the reference after LAT_STALE_US so it does not
// inflate the next measurement.
#define LAT_STALE_US 100000

static portMUX_TYPE lat_mux = portMUX_INITIALIZER_UNLOCKED;
static int64_t lat_pending_us;
static struct {
    uint32_t count, le8, le16, over16;
    uint64_t sum_us;
    uint32_t max_us;
} lat;

static void lat_note_received(int64_t t_us) {
    if (lat_pending_us == 0 || t_us - lat_pending_us > LAT_STALE_US) {
        lat_pending_us = t_us;
    }
}

void orbit_report_sent() {
    if (lat_pending_us == 0) {
        return;
    }
    uint32_t d = (uint32_t) (esp_timer_get_time() - lat_pending_us);
    lat_pending_us = 0;
    portENTER_CRITICAL(&lat_mux);
    lat.count++;
    lat.sum_us += d;
    if (d > lat.max_us) {
        lat.max_us = d;
    }
    if (d <= 8000) {
        lat.le8++;
    } else if (d <= 16000) {
        lat.le16++;
    } else {
        lat.over16++;
    }
    portEXIT_CRITICAL(&lat_mux);
}

void orbit_request_download_mode(const char* why) {
    download_mode_reason = why;
}

static void print_start() {
    olog("M1 START idf=%s app=%s upstream=%s config_size=%d descriptor=%u vid=%04x pid=%04x "
         "max_devs=%d conn_itvl=6(7.50ms) lvgl_reserve=%s\n",
         esp_get_idf_version(), esp_app_get_description()->version, ORBIT_UPSTREAM_COMMIT,
         PERSISTED_CONFIG_SIZE, our_descriptor_number, 0xCAFE, 0xBAF2, ORBIT_MAX_DEVS,
         lvgl_reserve ? "ok" : "FAILED");
}

// Upstream ticks the core once per millisecond (decision I2: esp_timer).
static void tick_cb(void* arg) {
    tick_pending = true;
    xTaskNotifyGive(main_task);
}

static void set_line(display_line_t* l, uint16_t color, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l->text, sizeof(l->text), fmt, ap);
    va_end(ap);
    l->color = color;
}

#define WHITE  DISPLAY_RGB(255, 255, 255)
#define GREEN  DISPLAY_RGB(0, 255, 0)
#define YELLOW DISPLAY_RGB(255, 255, 0)
#define GREY   DISPLAY_RGB(160, 160, 160)

// One line per device per second, e.g.
// M1 DEV t=12 D0 addr=..:3a:5f h=1 itvl=6(7.50ms) lat=0 to=400 enc=1 subs=3 rpt=120 maxgap=9.1ms gaps<=8/16/32/>32=100/18/1/0 total=1440 disc=0
static void report_device(int i, double t, const orbit_dev_stats_t* s, display_line_t* lines) {
    if (!s->connected) {
        olog("M1 DEV t=%.0f D%d none total=%lu disc=%lu\n", t, i, (unsigned long) s->total_reports,
             (unsigned long) s->disconnects);
        set_line(&lines[0], GREY, "D%d --", i);
        set_line(&lines[1], GREY, " ");
        return;
    }
    struct ble_gap_conn_desc desc = {};
    ble_gap_conn_find(s->conn_handle, &desc);
    double itvl_ms = desc.conn_itvl * 1.25;
    olog("M1 DEV t=%.0f D%d addr=..:%02x:%02x h=%u itvl=%u(%.2fms) lat=%u to=%u enc=%d subs=%d "
         "rpt=%lu maxgap=%.1fms gaps<=8/16/32/>32=%lu/%lu/%lu/%lu total=%lu disc=%lu\n",
         t, i, s->addr_lo[1], s->addr_lo[0], s->conn_handle, desc.conn_itvl, itvl_ms, desc.conn_latency,
         desc.supervision_timeout, s->encrypted, s->subscribed, (unsigned long) s->reports, s->max_gap_us / 1000.0,
         (unsigned long) s->gaps[GAP_LE_8MS], (unsigned long) s->gaps[GAP_LE_16MS],
         (unsigned long) s->gaps[GAP_LE_32MS], (unsigned long) s->gaps[GAP_OVER_32MS],
         (unsigned long) s->total_reports, (unsigned long) s->disconnects);
    uint16_t c = desc.conn_itvl == 6 ? GREEN : YELLOW;
    set_line(&lines[0], c, "D%d %02X%02X %.2fMS", i, s->addr_lo[1], s->addr_lo[0], itvl_ms);
    set_line(&lines[1], WHITE, "L%u %s S%d R%lu", desc.conn_latency, s->encrypted ? "ENC" : "RAW", s->subscribed,
             (unsigned long) s->reports);
}

// SUM, DEV and LAT lines and the screen, once a second, on CPU0 at low priority.
static void status_task(void* arg) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        orbit_ble_poll();
        double t = orbit_now_s();
        bool mounted = tud_mounted();
        bool susp = tud_suspended();
        int conn = orbit_ble_connected_count();
        uint32_t lost = orbit_ble_take_lost();
        // Internal RAM only (A9). The M5Dial has no PSRAM, but say so explicitly.
        unsigned heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        unsigned heap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        olog("M1 SUM t=%.0f conn=%d/%d scan=%d pairing=%d usb=%s boot_protocol=%d heap_free=%u heap_min=%u lost=%lu\n",
             t, conn, ORBIT_MAX_DEVS, orbit_ble_scanning(), orbit_ble_pairing(),
             !mounted ? "none" : susp ? "suspended" : "mounted", boot_protocol_keyboard, heap_free, heap_min,
             (unsigned long) lost);

        display_line_t lines[DISPLAY_ROWS] = {};
        set_line(&lines[0], WHITE, "ORBIT M1 %s", esp_app_get_description()->version);
        set_line(&lines[1], mounted && !susp ? GREEN : YELLOW, "USB %s %s", !mounted ? "--" : susp ? "SUSP" : "OK",
                 boot_protocol_keyboard ? "BOOT" : "");
        set_line(&lines[2], orbit_ble_pairing() ? YELLOW : WHITE, "%s %d/%d",
                 orbit_ble_pairing() ? "PAIRING" : orbit_ble_scanning() ? "SCAN" : "IDLE", conn, ORBIT_MAX_DEVS);
        orbit_dev_stats_t st;
        for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
            orbit_ble_take_stats(i, &st);
            report_device(i, t, &st, &lines[4 + i * 2]);
        }

        portENTER_CRITICAL(&lat_mux);
        auto l = lat;
        lat = {};
        portEXIT_CRITICAL(&lat_mux);
        if (l.count > 0) {
            olog("M1 LAT t=%.0f n=%lu avg=%.2fms max=%.2fms <=8/<=16/>16=%lu/%lu/%lu\n", t, (unsigned long) l.count,
                 l.sum_us / 1000.0 / l.count, l.max_us / 1000.0, (unsigned long) l.le8, (unsigned long) l.le16,
                 (unsigned long) l.over16);
            set_line(&lines[8], l.max_us <= 3000 ? GREEN : YELLOW, "LAT %.1f MAX%.1f", l.sum_us / 1000.0 / l.count,
                     l.max_us / 1000.0);
        }

        orbit_log_stats_t ls;
        orbit_log_get_stats(&ls);
        set_line(&lines[9], ls.connected ? GREEN : GREY, "LOG %s %luK C%lu", ls.connected ? "DTR" : "--",
                 (unsigned long) (ls.sent / 1024), (unsigned long) ls.completed);
        set_line(&lines[10], GREY, "HEAP %uK MIN %uK", heap_free / 1024, heap_min / 1024);
        char ev[DISPLAY_COLS + 1];
        orbit_ble_last_event(ev, sizeof(ev));
        set_line(&lines[11], GREY, "%s", ev);
        display_show(lines);
    }
}

static void show_download_mode() {
    display_line_t lines[DISPLAY_ROWS] = {};
    set_line(&lines[5], YELLOW, "DOWNLOAD MODE");
    set_line(&lines[7], WHITE, "READY TO FLASH");
    display_show(lines);
}

static void main_loop(void* arg) {
    orbit_usb_init();

    esp_timer_create_args_t tick_args = {};
    tick_args.callback = tick_cb;
    tick_args.name = "tick";
    esp_timer_handle_t tick;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 1000));

    for (;;) {
        // Woken by the 1 ms tick at the latest.
        ulTaskNotifyTake(pdTRUE, 1);

        tud_task_ext(0, false);
        orbit_log_pump();

        // From the BLE task (decision I2: only this task calls the core).
        orbit_disconnect_t disc;
        while (orbit_ble_take_disconnect(&disc)) {
            olog("M1 EVT t=%.3f D%u device_disconnected_callback\n", orbit_now_s(), disc.slot);
            device_disconnected_callback(disc.slot);
        }
        static orbit_report_map_t map;
        while (orbit_ble_take_report_map(&map)) {
            // vid/pid 1/1 and itf_num 0, as upstream's Bluetooth build.
            device_connected_callback(map.interface, 1, 1, map.hub_port);
            parse_descriptor(1, 1, map.data, map.len, map.interface, 0);
            olog("M1 EVT t=%.3f D%u descriptor parsed len=%u hub_port=%u\n", orbit_now_s(), map.interface >> 8,
                 map.len, map.hub_port);
        }
        orbit_report_t rep;
        if (orbit_ble_take_report(&rep)) {
            lat_note_received(rep.t_us);
            handle_received_report(rep.data, rep.len, rep.interface, rep.report_id);
        }

        if (their_descriptor_updated) {
            update_their_descriptor_derivates();
            their_descriptor_updated = false;
        }
        if (tick_pending.exchange(false)) {
            process_mapping(true);
        }
        if (boot_protocol_updated) {
            parse_our_descriptor();
            boot_protocol_updated = false;
            config_updated = true;
        }
        if (resume_pending) {
            resume_pending = false;
            suspended = false;
        }
        if (config_updated) {
            set_mapping_from_config();
            config_updated = false;
        }
        if (tud_hid_n_ready(0) || tud_suspended()) {
            send_report(do_send_report);
        }
        if (monitor_enabled && tud_hid_n_ready(1)) {
            send_monitor_report(do_send_report);
        }
        if (our_descriptor->main_loop_task != nullptr) {
            our_descriptor->main_loop_task();
        }
        if (need_to_persist_config) {
            int64_t t0 = esp_timer_get_time();
            persist_config_return_code = persist_config();
            olog("M1 EVT t=%.3f persist_config took %.1f ms\n", orbit_now_s(), (esp_timer_get_time() - t0) / 1000.0);
            need_to_persist_config = false;
        }

        const char* why = download_mode_reason.exchange(nullptr);
        if (why != nullptr) {
            olog("M1 EVT t=%.3f entering download mode (%s)\n", orbit_now_s(), why);
            show_download_mode();
            // Let the control transfer finish and the log drain, then let go of USB.
            int64_t until = esp_timer_get_time() + 300000;
            while (esp_timer_get_time() < until) {
                tud_task_ext(0, false);
                orbit_log_pump();
                vTaskDelay(1);
            }
            tud_disconnect();
            vTaskDelay(pdMS_TO_TICKS(100));
            orbit_enter_download_mode();
        }
    }
}

extern "C" void app_main() {
    gpio_set_direction((gpio_num_t) PIN_POWER_HOLD, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t) PIN_POWER_HOLD, 1);

    // Button held at power-up: straight into the ROM's download mode, before
    // TinyUSB takes the USB pins (brief 4.1, second way back).
    gpio_set_direction((gpio_num_t) PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t) PIN_BUTTON, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(20));
    if (gpio_get_level((gpio_num_t) PIN_BUTTON) == 0) {
        display_init();
        show_download_mode();
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
    }

    lvgl_reserve = heap_caps_malloc(LVGL_RESERVE_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    ESP_ERROR_CHECK(orbit_storage_init());
    my_mutexes_init();
    static uint8_t persisted[PERSISTED_CONFIG_SIZE];
    esp_err_t cfg_err = orbit_storage_load_config(persisted, sizeof(persisted));
    load_config(persisted);
    our_descriptor = &our_descriptors[our_descriptor_number];
    parse_our_descriptor();
    set_mapping_from_config();

    display_init();
    orbit_log_init(print_start);
    print_start();
    olog("M1 EVT t=%.3f config loaded from NVS err=0x%x (non-zero: defaults)\n", orbit_now_s(), cfg_err);

    xTaskCreatePinnedToCore(main_loop, "main_loop", 8192, NULL, 10, &main_task, 1);
    orbit_ble_start(main_task);
    xTaskCreatePinnedToCore(status_task, "status", 5120, NULL, 1, NULL, 0);
}
