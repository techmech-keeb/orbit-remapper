idf_component_register(SRCS "main.c" "ble_central.c" "display.c" "font5x7.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES bt nvs_flash esp_driver_gpio esp_driver_spi esp_lcd esp_timer esp_app_format)
