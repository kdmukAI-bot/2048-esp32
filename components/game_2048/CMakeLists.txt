idf_component_register(
    SRCS "game_logic.c" "game_ui.c" "game_gesture.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_lv_port" "lvgl__lvgl" "nvs_flash"
)
