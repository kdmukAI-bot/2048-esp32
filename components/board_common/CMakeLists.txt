set(SRC_FILES
    "board_init.c"
    "board_i2c.c"
    "board_backlight.c"
    "board_pmic.cpp"
    "board_display_axs15231b.c"
    "board_display_st7796.c"
    "board_display_st7789.c"
    "board_touch_axs15231b.c"
    "board_touch_ft6336.c"
    "board_touch_cst816d.c"
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES "esp_lcd" "driver" "esp_adc" "esp_lvgl_port" "lvgl"
             "espressif__esp_lcd_axs15231b" "espressif__esp_io_expander_tca9554"
             "espressif__esp_lcd_st7796"
             "espressif__esp_lcd_touch_ft5x06" "espressif__esp_lcd_touch_cst816s"
             "XPowersLib"
)

# Board config header: selected at build time via BOARD variable
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_SOURCE_DIR}/boards/${BOARD}"
)
