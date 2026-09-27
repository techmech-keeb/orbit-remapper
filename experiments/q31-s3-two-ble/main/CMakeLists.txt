set(srcs "main.c" "ble_central.c" "display.c" "font5x7.c")
if(CONFIG_Q31_PKT_LOG)
    list(APPEND srcs "pkt_log.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES bt nvs_flash esp_driver_gpio esp_driver_spi esp_lcd esp_timer esp_app_format)

if(CONFIG_Q31_PKT_LOG)
    # pkt_log.c sees every ACL packet between the NimBLE host and the controller.
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=ble_transport_to_hs_acl_impl"
        "-Wl,--wrap=esp_vhci_host_send_packet")
endif()
