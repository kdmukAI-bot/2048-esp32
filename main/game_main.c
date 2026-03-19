/**
 * Shared entry point for all board targets.
 *
 * Called by app_main() (ESP32 boards, defined in board_init.c)
 * or main() (desktop, defined in boards/desktop/board_init.c).
 */
#include "board.h"
#include "game_ui.h"

#ifndef DESKTOP_BUILD
#include "esp_lvgl_port.h"
#include "esp_log.h"
static const char *TAG = "2048";
#endif

void game_main(void)
{
    lv_display_t *disp;
    lv_indev_t *touch;

    board_init(&disp, &touch);

#ifndef DESKTOP_BUILD
    if (lvgl_port_lock(0)) {
        game_ui_init(touch);
        lvgl_port_unlock();
    }
    ESP_LOGI(TAG, "Game started.");
#else
    game_ui_init(touch);
#endif

    board_run();  /* never returns */
}
