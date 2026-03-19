idf_component_register(
    SRCS "game_main.c"
    INCLUDE_DIRS "."
    REQUIRES "board_common" "esp_lvgl_port" "game_2048"
)
