idf_component_register(SRCS "main.c" "scene.c" "touch_input.c" "../../../common/pin_gfx.c"
 "../../../common/pin_board.c" "../../../common/app_switcher.c"
 INCLUDE_DIRS "." "../../../common"
 REQUIRES nvs_flash esp_lcd esp_driver_i2c esp_driver_spi esp_driver_gpio esp_timer esp_system
 esp_app_format app_update heap)
