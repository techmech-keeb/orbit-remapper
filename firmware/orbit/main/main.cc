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
#include "tusb.h"

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

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

void orbit_report_sent() {
}

void orbit_request_download_mode(const char* why) {
    download_mode_reason = why;
}

void pair_new_device() {
    olog("M1 EVT t=%.3f pair_new_device: no Bluetooth in this build\n", orbit_now_s());
}

void clear_bonds() {
    olog("M1 EVT t=%.3f clear_bonds: no Bluetooth in this build\n", orbit_now_s());
}

static void print_start() {
    olog("M1 START idf=%s app=%s upstream=%s config_size=%d descriptor=%u vid=%04x pid=%04x "
         "lvgl_reserve=%s\n",
         esp_get_idf_version(), esp_app_get_description()->version, ORBIT_UPSTREAM_COMMIT,
         PERSISTED_CONFIG_SIZE, our_descriptor_number, 0xCAFE, 0xBAF2, lvgl_reserve ? "ok" : "FAILED");
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

// SUM line and screen, once a second, on CPU0 at low priority.
static void status_task(void* arg) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        bool mounted = tud_mounted();
        bool susp = tud_suspended();
        olog("M1 SUM t=%.0f usb=%s boot_protocol=%d heap_free=%u heap_min=%u\n", orbit_now_s(),
             !mounted ? "none" : susp ? "suspended" : "mounted", boot_protocol_keyboard,
             (unsigned) esp_get_free_heap_size(), (unsigned) esp_get_minimum_free_heap_size());

        display_line_t lines[DISPLAY_ROWS] = {};
        set_line(&lines[1], WHITE, "ORBIT M1");
        set_line(&lines[2], GREY, "%s", esp_app_get_description()->version);
        set_line(&lines[4], mounted && !susp ? GREEN : YELLOW, "USB %s",
                 !mounted ? "--" : susp ? "SUSPEND" : "OK");
        set_line(&lines[5], WHITE, "%s", boot_protocol_keyboard ? "BOOT KBD" : " ");
        orbit_log_stats_t ls;
        orbit_log_get_stats(&ls);
        set_line(&lines[6], ls.connected ? GREEN : GREY, "LOG %s %luK C%lu", ls.connected ? "DTR" : "--",
                 (unsigned long) (ls.sent / 1024), (unsigned long) ls.completed);
        if (ls.lost != 0) {
            set_line(&lines[9], YELLOW, "LOG LOST %luK", (unsigned long) (ls.lost / 1024));
        }
        set_line(&lines[7], GREY, "HEAP %uK MIN %uK", (unsigned) (esp_get_free_heap_size() / 1024),
                 (unsigned) (esp_get_minimum_free_heap_size() / 1024));
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
    xTaskCreatePinnedToCore(status_task, "status", 4096, NULL, 1, NULL, 0);
}
