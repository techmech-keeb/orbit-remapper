// The functions upstream's core expects from the platform (platform.h and
// the rest of what firmware-bluetooth/src/main.cc provides at 51ab8b3).

#include <cstring>

#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "nvs.h"
#include "soc/rtc_cntl_reg.h"
#include "tusb.h"

#include "globals.h"
#include "platform.h"
#include "remapper.h"

#include "log.h"
#include "orbit.h"
#include "storage.h"

static SemaphoreHandle_t mutexes[(uint8_t) MutexId::N];

void do_persist_config(uint8_t* buffer) {
    esp_err_t err = orbit_storage_save_config(buffer, PERSISTED_CONFIG_SIZE);
    olog("M1 EVT t=%.3f config saved err=0x%x\n", orbit_now_s(), err);
}

void reset_to_bootloader() {
    orbit_request_download_mode("config tool");
}

void flash_b_side() {
}

void my_mutexes_init() {
    for (int i = 0; i < (int8_t) MutexId::N; i++) {
        mutexes[i] = xSemaphoreCreateMutex();
    }
}

void my_mutex_enter(MutexId id) {
    xSemaphoreTake(mutexes[(uint8_t) id], portMAX_DELAY);
}

void my_mutex_exit(MutexId id) {
    xSemaphoreGive(mutexes[(uint8_t) id]);
}

uint64_t get_time() {
    return esp_timer_get_time();
}

// Upstream uses the Pico's board ID. Here a hash of the factory MAC, so the
// USB serial number stays stable without showing the MAC itself.
uint64_t get_unique_id() {
    uint8_t mac[6] = { 0 };
    esp_efuse_mac_get_default(mac);
    uint64_t h = 0xcbf29ce484222325ULL; // FNV-1a
    for (int i = 0; i < 6; i++) {
        h = (h ^ mac[i]) * 0x100000001b3ULL;
    }
    return h;
}

uint32_t get_gpio_valid_pins_mask() {
    return 0;
}

void set_gpio_inout_masks(uint32_t in_mask, uint32_t out_mask) {
}

void interval_override_updated() {
}

// As in upstream's Bluetooth build, output reports to the devices are not sent.
void queue_out_report(uint16_t interface, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
}

void queue_set_feature_report(uint16_t interface, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
}

void queue_get_feature_report(uint16_t interface, uint8_t report_id, uint8_t len) {
}

// How long the bus stays idle before the port reappears in download mode.
// Through a hub a quick detach and reattach was not recognised (A1 report,
// 2026-09-27); the value is a guess.
#define DOWNLOAD_MODE_DETACH_MS 500

void orbit_usj_detach(int ms) {
    const usb_serial_jtag_pull_override_vals_t detached = {
        .dp_pu = false,
        .dm_pu = false,
        .dp_pd = true,
        .dm_pd = true,
    };
    usb_serial_jtag_ll_phy_enable_pull_override(&detached);
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void orbit_usj_attach() {
    usb_serial_jtag_ll_phy_disable_pull_override();
}

void orbit_enter_download_mode() {
    // Route the internal USB PHY back from the OTG controller (TinyUSB) to the
    // USB Serial/JTAG, which the ROM's download mode talks through. Keep the
    // bus idle for a while, then attach and reboot at once: the USB
    // Serial/JTAG enumerates in hardware and stays up across esp_restart().
    orbit_usj_detach(0);
    usb_serial_jtag_ll_phy_enable_external(false);
    usb_serial_jtag_ll_phy_enable_pad(true);
    vTaskDelay(pdMS_TO_TICKS(DOWNLOAD_MODE_DETACH_MS));
    orbit_usj_attach();
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}
