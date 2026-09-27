// Q31 experiment: can the original M5Dial (ESP32-S3) keep a BLE keyboard and
// a BLE mouse both at a 7.5 ms connection interval?
// See docs/drafts/2026-09-26/q31-experiment-brief.md.

#include <stdarg.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "nvs_flash.h"
#include "ble_central.h"
#include "display.h"

#define PIN_POWER_HOLD 46 // keeps the M5Dial on when running from battery
#define PIN_BUTTON     42 // screen push button, low when pressed

#define REPORT_PERIOD_US 1000000

#if CONFIG_Q31_ACCEPT_PEER_UPDATE
#define ACCEPT_PEER_UPDATE 1
#else
#define ACCEPT_PEER_UPDATE 0
#endif

#define WHITE  DISPLAY_RGB(255, 255, 255)
#define GREEN  DISPLAY_RGB(0, 255, 0)
#define YELLOW DISPLAY_RGB(255, 255, 0)
#define RED    DISPLAY_RGB(255, 64, 64)
#define GREY   DISPLAY_RGB(160, 160, 160)

#if CONFIG_Q31_ITVL_AT_CONNECT
#define MODE_NAME "at-connect"
#define MODE_SHORT "AT-CONN"
#else
#define MODE_NAME "after-connect"
#define MODE_SHORT "AFTER"
#endif

static void set_line(display_line_t *l, uint16_t color, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l->text, sizeof(l->text), fmt, ap);
    va_end(ap);
    l->color = color;
}

// One status line per device per second, e.g.
// Q31 DEV t=12 D0 addr=..:3a:5f h=1 itvl=6(7.50ms) lat=0 to=400 enc=1 subs=3 rpt=120 maxgap=9.1ms gaps<=8/16/32/>32=100/18/1/0 total=1440 disc=0
static void report_device(int i, double t, const q31_dev_stats_t *s, display_line_t *lines)
{
    if (!s->connected) {
        printf("Q31 DEV t=%.0f D%d none total=%lu disc=%lu\n", t, i, (unsigned long)s->total_reports,
               (unsigned long)s->disconnects);
        set_line(&lines[0], RED, "D%d --", i);
        set_line(&lines[1], GREY, "DISC %lu", (unsigned long)s->disconnects);
        set_line(&lines[2], GREY, " ");
        return;
    }

    struct ble_gap_conn_desc desc = {0};
    ble_gap_conn_find(s->conn_handle, &desc);
    double itvl_ms = desc.conn_itvl * 1.25;
    printf("Q31 DEV t=%.0f D%d addr=..:%02x:%02x h=%u itvl=%u(%.2fms) lat=%u to=%u enc=%d subs=%d "
           "rpt=%lu maxgap=%.1fms gaps<=8/16/32/>32=%lu/%lu/%lu/%lu total=%lu disc=%lu\n",
           t, i, s->addr_lo[1], s->addr_lo[0], s->conn_handle, desc.conn_itvl, itvl_ms, desc.conn_latency,
           desc.supervision_timeout, s->encrypted, s->subscribed, (unsigned long)s->reports,
           s->max_gap_us / 1000.0, (unsigned long)s->gaps[GAP_LE_8MS], (unsigned long)s->gaps[GAP_LE_16MS],
           (unsigned long)s->gaps[GAP_LE_32MS], (unsigned long)s->gaps[GAP_OVER_32MS],
           (unsigned long)s->total_reports, (unsigned long)s->disconnects);

    uint16_t c = desc.conn_itvl == CONFIG_Q31_ITVL_UNITS ? GREEN : YELLOW;
    set_line(&lines[0], c, "D%d %02X%02X %s", i, s->addr_lo[1], s->addr_lo[0], s->encrypted ? "ENC" : "RAW");
    set_line(&lines[1], c, "%.2fMS L%u T%u", itvl_ms, desc.conn_latency, desc.supervision_timeout * 10);
    set_line(&lines[2], WHITE, "R%lu MAX%.1f", (unsigned long)s->reports, s->max_gap_us / 1000.0);
}

static void report_task(void *arg)
{
    int64_t next = esp_timer_get_time() + REPORT_PERIOD_US;
    for (;;) {
        int64_t wait = next - esp_timer_get_time();
        if (wait > 0) {
            vTaskDelay(pdMS_TO_TICKS(wait / 1000));
        }
        next += REPORT_PERIOD_US;
        double t = esp_timer_get_time() / 1e6;

        display_line_t lines[DISPLAY_ROWS] = {0};
        q31_dev_stats_t stats[Q31_MAX_DEVS];
        int connected = 0;
        for (int i = 0; i < Q31_MAX_DEVS; i++) {
            ble_central_take_stats(i, &stats[i]);
            connected += stats[i].connected;
        }
        bool scanning = ble_central_scanning();
        printf("Q31 SUM t=%.0f conn=%d/%d scan=%d\n", t, connected, Q31_MAX_DEVS, scanning);

        set_line(&lines[1], WHITE, "Q31 %s", MODE_SHORT);
        set_line(&lines[2], WHITE, "%s %d/%d T%.0f", scanning ? "SCAN" : "IDLE", connected, Q31_MAX_DEVS, t);
        for (int i = 0; i < Q31_MAX_DEVS; i++) {
            report_device(i, t, &stats[i], &lines[4 + i * 4]);
        }
        char ev[DISPLAY_COLS + 1];
        ble_central_last_event(ev, sizeof(ev));
        set_line(&lines[11], GREY, "%s", ev);
        display_show(lines);
    }
}

void app_main(void)
{
    gpio_set_direction(PIN_POWER_HOLD, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_POWER_HOLD, 1);
    gpio_set_direction(PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(20));
    bool clear_bonds = gpio_get_level(PIN_BUTTON) == 0;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    display_init();

    // Everything needed to reproduce the run goes into the first line of the log.
    printf("Q31 START idf=%s app=%s mode=%s itvl_req=%d(%.2fms) to=%d ce_len=%d accept_peer_update=%d "
           "clear_bonds=%d\n",
           esp_get_idf_version(), esp_app_get_description()->version, MODE_NAME, CONFIG_Q31_ITVL_UNITS,
           CONFIG_Q31_ITVL_UNITS * 1.25, CONFIG_Q31_SUPERVISION_TIMEOUT, CONFIG_Q31_CE_LEN,
           ACCEPT_PEER_UPDATE, clear_bonds);

    ble_central_start(clear_bonds);

    // NimBLE and the controller run on core 0; keep screen drawing and log
    // formatting off that core so they do not disturb the measurement.
    xTaskCreatePinnedToCore(report_task, "report", 6144, NULL, 1, NULL, 1);
}
