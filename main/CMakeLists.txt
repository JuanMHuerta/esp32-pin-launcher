idf_component_register(SRCS "main.c" "menu_render.c" "sdcard.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_lcd esp_driver_spi esp_driver_gpio
                                esp_driver_usb_serial_jtag app_update esp_timer heap
                                fatfs sdmmc esp_driver_sdmmc)
