#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// NVS for the remapper config (one blob; config sets come in M2).
esp_err_t orbit_storage_init();
// Fills buf; on any failure it is zeroed so that load_config() falls back to defaults.
esp_err_t orbit_storage_load_config(uint8_t* buf, size_t len);
esp_err_t orbit_storage_save_config(const uint8_t* buf, size_t len);
