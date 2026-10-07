idf_component_register(SRCS "main.cc" "commands.cc" "tool.cc" "usb.cc" "typist.cc" "platform.cc" "storage.cc" "log.c" "ble.c" "ledger.c" "ui.c" "display.c" "font5x7.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES hid_remapper_core bt usb nvs_flash esp_timer esp_app_format
                                     esp_driver_gpio esp_driver_spi esp_driver_pcnt esp_lcd hal
                                     esp_lvgl_port lvgl)

# TinyUSB reads its configuration from main/tusb/tusb_config.h.
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tusb")
