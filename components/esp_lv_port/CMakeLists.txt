idf_component_register(
    SRCS "lv_port.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_lcd" "lvgl__lvgl"
    PRIV_REQUIRES "esp_timer"
)
