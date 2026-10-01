idf_component_register(SRCS "main.c" "board.c" "mech.c" "paint.c" "../../../common/app_switcher.c"
                       INCLUDE_DIRS "." "../../../common"
                       REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_timer
                                esp_driver_usb_serial_jtag esp_system esp_app_format app_update heap)
