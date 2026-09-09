idf_component_register(SRCS "src/bsp_camera.c"
                            "src/bsp_display_stub.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_driver_i2c esp_lcd)
