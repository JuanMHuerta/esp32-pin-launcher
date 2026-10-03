idf_component_register(SRCS "main.c" "renderer.c" "motion.c" "../../../common/app_switcher.c"
                       INCLUDE_DIRS "." "../../../common"
                       REQUIRES nvs_flash esp_lcd driver esp_driver_i2c esp_timer app_update)
