idf_component_register(SRCS "main.c" "board.c" "pet.c" "paint.c" "input.c" "../../../common/app_switcher.c"
                       INCLUDE_DIRS "." "../../../common"
                       REQUIRES nvs_flash esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c
                                esp_timer esp_driver_usb_serial_jtag esp_system esp_app_format app_update)
