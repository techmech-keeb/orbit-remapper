#include <cstring>

#include "nvs.h"
#include "nvs_flash.h"

#include "storage.h"

#define NS  "orbit"
#define KEY "config"

esp_err_t orbit_storage_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t orbit_storage_load_config(uint8_t* buf, size_t len) {
    memset(buf, 0, len);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    size_t got = len;
    err = nvs_get_blob(h, KEY, buf, &got);
    nvs_close(h);
    if (err == ESP_OK && got != len) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err != ESP_OK) {
        memset(buf, 0, len);
    }
    return err;
}

esp_err_t orbit_storage_save_config(const uint8_t* buf, size_t len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, KEY, buf, len);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
