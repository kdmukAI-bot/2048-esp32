set(SRC_FILES
    "hw_display.c"
    "hw_i2c.c"
    "hw_pmic.cpp"
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES "esp_lcd" "driver" "esp_adc" "espressif__esp_lcd_axs15231b" "XPowersLib"
)
