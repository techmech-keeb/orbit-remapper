#include <cstdio>
#include <cstring>

#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "log.h"
#include "orbit.h"
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

// NimBLE's NVS store (ble_store_nvs.c) keeps one blob per record under
// "<kind>_<index>"; "kind" is peer_sec, our_sec, cccd_sec, csfc_sec,
// p_dev_rec, ead_sec, local_irk or rpa_rec. Records of one kind are the same
// size, so per kind the count and the bytes of one record say it all.
static void log_namespace(const char* ns) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) {
        olog("M1 NVS t=%.3f namespace %s: none\n", orbit_now_s(), ns);
        return;
    }
    size_t entries = 0;
    nvs_get_used_entry_count(h, &entries);
    char line[200];
    int pos = snprintf(line, sizeof(line), "M1 NVS t=%.3f namespace %s: entries=%u", orbit_now_s(), ns,
                       (unsigned) entries);

    struct {
        char kind[NVS_KEY_NAME_MAX_SIZE];
        unsigned count;
        size_t bytes;
    } kinds[8];
    int n_kinds = 0;
    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find_in_handle(h, NVS_TYPE_ANY, &it);
    while (res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        size_t bytes = 0;
        if (info.type == NVS_TYPE_BLOB) {
            nvs_get_blob(h, info.key, NULL, &bytes);
        }
        // Strip NimBLE's "_<index>" so that records of one kind share a slot.
        char kind[NVS_KEY_NAME_MAX_SIZE];
        strncpy(kind, info.key, sizeof(kind) - 1);
        kind[sizeof(kind) - 1] = '\0';
        char* us = strrchr(kind, '_');
        if (us != NULL && us != kind && us[1] >= '0' && us[1] <= '9') {
            *us = '\0';
        }
        int k = 0;
        while (k < n_kinds && strcmp(kinds[k].kind, kind) != 0) {
            k++;
        }
        if (k == n_kinds && n_kinds < (int) (sizeof(kinds) / sizeof(kinds[0]))) {
            strcpy(kinds[n_kinds].kind, kind);
            kinds[n_kinds].count = 0;
            kinds[n_kinds].bytes = bytes;
            n_kinds++;
        }
        if (k < n_kinds) {
            kinds[k].count++;
        }
        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(h);
    for (int k = 0; k < n_kinds && pos < (int) sizeof(line); k++) {
        pos += snprintf(line + pos, sizeof(line) - pos, " %s=%ux%uB", kinds[k].kind, kinds[k].count,
                        (unsigned) kinds[k].bytes);
    }
    olog("%s\n", line);
}

void orbit_storage_log_usage() {
    nvs_stats_t st = {};
    esp_err_t err = nvs_get_stats(NULL, &st);
    // An NVS entry is 32 bytes; a blob costs 2 entries plus one per 32 bytes of data.
    olog("M1 NVS t=%.3f err=0x%x entries used=%u free=%u available=%u total=%u (32 B each) namespaces=%u "
         "max_bonds=%d accept_list_max=%d max_conns=%d\n",
         orbit_now_s(), err, (unsigned) st.used_entries, (unsigned) st.free_entries,
         (unsigned) st.available_entries, (unsigned) st.total_entries, (unsigned) st.namespace_count,
         CONFIG_BT_NIMBLE_MAX_BONDS, CONFIG_BT_NIMBLE_WHITELIST_SIZE, CONFIG_BT_NIMBLE_MAX_CONNECTIONS);
    log_namespace(NS);
    log_namespace("nimble_bond");
}
