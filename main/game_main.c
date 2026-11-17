/**
 * Application entry point and game startup.
 *
 * Defines app_main() for ESP32 boards, which initialises board hardware via
 * esp-board-common, then launches the game UI. The desktop simulator gets game_main()
 * instead, called from main() in boards/desktop/board_init.c.
 */
#include "board.h"
#include "game_ui.h"

#ifndef DESKTOP_BUILD
#include "board_backlight.h"
#include "sdkconfig.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_ESP32P4_REV_MIN_FULL >= 300
#include "soc/hp_system_reg.h"
#include "soc/soc.h"
#endif
static const char *TAG = "2048";
#endif

#ifndef DESKTOP_BUILD
void app_main(void)
#else
/* main() stays in boards/desktop/board_init.c: it includes SDL.h, which on Windows
 * renames main() so SDL2main can wrap it. */
void game_main(void)
#endif
{
    lv_display_t *disp;
    lv_indev_t *touch;

#if !defined(DESKTOP_BUILD) && CONFIG_IDF_TARGET_ESP32P4 && CONFIG_ESP32P4_REV_MIN_FULL >= 300
    /* ESP32-P4 v3.x can hold a core load or store on the PSRAM path for longer than the
     * core bus timeout (65535 cycles, the register maximum). The access would complete,
     * but the timeout turns it into an asynchronous access fault that cannot be retried.
     * Disable both core bus timeouts before the display and LVGL start using their PSRAM
     * buffers. Same fix as the SeedSigner MicroPython builder and Kern (odudex/kern
     * 3ad82dd); pre-v3 builds leave the timeouts alone. */
    REG_CLR_BIT(HP_SYSTEM_CORE_DBUS_TIMEOUT_REG, HP_SYSTEM_CORE_DBUS_TIMEOUT_EN);
    REG_CLR_BIT(HP_SYSTEM_CORE_IBUS_TIMEOUT_REG, HP_SYSTEM_CORE_IBUS_TIMEOUT_EN);
#endif

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

    board_run();  /* never returns on ESP32; on desktop, returns when the window closes */
}
