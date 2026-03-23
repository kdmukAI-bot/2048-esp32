idf_component_register(
    SRCS "game_main.c"
    INCLUDE_DIRS "."
    REQUIRES "esp-board-common" "esp_lvgl_port" "game_2048"
)
