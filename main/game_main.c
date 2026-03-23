/**
 * Application entry point and game startup.
 *
 * Defines app_main() for ESP32 boards. Initialises board hardware via
 * esp-board-common, then launches the game UI.
 */
#include "board.h"
#include "board_backlight.h"
#include "game_ui.h"

#ifndef DESKTOP_BUILD
#include "esp_lvgl_port.h"
#include "esp_log.h"
static const char *TAG = "2048";
#endif

#ifndef DESKTOP_BUILD
void app_main(void)
#else
int main(void)
#endif
{
    lv_display_t *disp;
    lv_indev_t *touch;

    board_init(NULL, &disp, &touch);  /* NULL = portrait defaults */

#ifndef DESKTOP_BUILD
    if (lvgl_port_lock(0)) {
        game_ui_init(touch);
        lvgl_port_unlock();
    }
    /* Turn on backlight after first frame to avoid white flash */
    board_backlight_set(100);
    ESP_LOGI(TAG, "Game started.");
#else
    game_ui_init(touch);
#endif

    board_run();  /* never returns */

#ifdef DESKTOP_BUILD
    return 0;
#endif
}
