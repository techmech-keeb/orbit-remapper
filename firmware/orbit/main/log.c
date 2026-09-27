#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "log.h"

#define RING_SIZE 16384

static char ring[RING_SIZE];
static size_t head, tail; // free-running byte counters
static uint32_t lost;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static orbit_log_connect_cb_t connect_cb;

static void push(const char* s, size_t n)
{
    portENTER_CRITICAL_SAFE(&mux);
    if (RING_SIZE - (head - tail) < n) {
        lost += n;
    } else {
        for (size_t i = 0; i < n; i++) {
            ring[head++ % RING_SIZE] = s[i];
        }
    }
    portEXIT_CRITICAL_SAFE(&mux);
}

static void vlog(const char* fmt, va_list ap)
{
    char line[256];
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(line)) {
        n = sizeof(line) - 1;
        line[n - 1] = '\n';
    }
    push(line, n);
}

void olog(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(fmt, ap);
    va_end(ap);
}

// ESP-IDF's own ESP_LOGx output joins the same stream.
static int esp_log_hook(const char* fmt, va_list ap)
{
    vlog(fmt, ap);
    return 0;
}

static void log_task(void* arg)
{
    bool was_connected = false;
    for (;;) {
        bool connected = tud_cdc_connected();
        if (connected && !was_connected && connect_cb != NULL) {
            connect_cb();
        }
        was_connected = connected;
        if (!connected) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        uint32_t dropped;
        portENTER_CRITICAL(&mux);
        dropped = lost;
        lost = 0;
        portEXIT_CRITICAL(&mux);
        if (dropped != 0) {
            char msg[48];
            int n = snprintf(msg, sizeof(msg), "M1 LOG lost %lu byte(s)\n", (unsigned long)dropped);
            tud_cdc_write(msg, n);
        }

        bool wrote = false;
        for (;;) {
            char chunk[64];
            size_t n = 0;
            uint32_t room = tud_cdc_write_available();
            portENTER_CRITICAL(&mux);
            while (n < sizeof(chunk) && n < room && tail != head) {
                chunk[n++] = ring[tail++ % RING_SIZE];
            }
            portEXIT_CRITICAL(&mux);
            if (n == 0) {
                break;
            }
            tud_cdc_write(chunk, n);
            wrote = true;
        }
        if (wrote) {
            tud_cdc_write_flush();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void orbit_log_init(orbit_log_connect_cb_t on_connect)
{
    connect_cb = on_connect;
    esp_log_set_vprintf(esp_log_hook);
    // CPU0 at the lowest priority: never in the way of the main loop (CPU1).
    xTaskCreatePinnedToCore(log_task, "log", 3072, NULL, 1, NULL, 0);
}
