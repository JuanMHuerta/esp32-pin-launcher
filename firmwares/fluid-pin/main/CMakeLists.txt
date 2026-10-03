idf_component_register(SRCS "main.c" "board.c" "fluid.c" "render.c" "motion.c" "../../../common/app_switcher.c"
                       INCLUDE_DIRS "." "../../../common"
                       REQUIRES nvs_flash esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c
                                esp_timer esp_driver_usb_serial_jtag esp_system app_update)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -fno-math-errno)
foreach(option FLUID_DIAGNOSTICS FLUID_IMU_DIAGNOSTIC FLUID_SYNTHETIC_TEST FLUID_RENDER_SELF_TEST)
    if(DEFINED ${option})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE ${option}=${${option}})
    endif()
endforeach()
