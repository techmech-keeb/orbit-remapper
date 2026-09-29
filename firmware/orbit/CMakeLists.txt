cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# NimBLE's security manager, on "key missing" from a bonded device, starts
# a new pairing by itself (ble_sm.c, BLE_RESTART_PAIR) without ever
# posting the failure to ble.c. That let a device that had dropped its
# bond pair again outside pairing mode (1fd6c6a report). The option has no
# Kconfig entry, only an #ifndef default of 1 in esp_nimble_cfg.h. With it
# off, the failure reaches ble.c, which re-pairs only in pairing mode.
idf_build_set_property(COMPILE_DEFINITIONS "MYNEWT_VAL_BLE_RESTART_PAIR=0" APPEND)

project(orbit)
