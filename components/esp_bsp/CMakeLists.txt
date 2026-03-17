set(SRC_FILES
    "bsp_display.c"
    "bsp_touch.c"
    "bsp_i2c.c"
    "bsp_axp2101.cpp"
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES "esp_lcd" "driver" "esp_adc" "espressif__esp_lcd_axs15231b" "XPowersLib"
)
