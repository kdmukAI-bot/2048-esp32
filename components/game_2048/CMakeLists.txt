idf_component_register(
    SRCS "game_logic.c" "game_ui.c" "game_gesture.c"
         "../../fonts/font_clear_sans_bold_20.c"
         "../../fonts/font_clear_sans_bold_24.c"
         "../../fonts/font_clear_sans_bold_28.c"
         "../../fonts/font_clear_sans_bold_30.c"
         "../../fonts/font_clear_sans_bold_40.c"
         "../../fonts/font_clear_sans_bold_50.c"
         "../../fonts/font_clear_sans_regular_14.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_lvgl_port" "lvgl" "nvs_flash"
)
