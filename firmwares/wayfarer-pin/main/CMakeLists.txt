idf_component_register(SRCS "main.c" "scene.c" "assets_generated.c" "exterior_generated.c" "../../../common/app_switcher.c"
                       INCLUDE_DIRS "." "../../../common"
                       EMBED_FILES "assets/cockpit.rgb565"
                       REQUIRES nvs_flash esp_lcd esp_driver_spi esp_driver_gpio esp_timer
                                esp_system esp_app_format app_update heap)
