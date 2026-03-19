idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES "board_hw" "esp_lvgl_port" "game_2048"
)
