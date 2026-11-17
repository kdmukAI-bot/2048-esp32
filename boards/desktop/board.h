/**
 * Board interface for the desktop simulator.
 *
 * ESP32 builds get board.h from esp-board-common, but that header pulls in ESP-IDF
 * types. This one declares only the subset main/game_main.c and board_init.c use, with
 * the same signatures, plus game_main(), which the desktop main() calls.
 */
#pragma once

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resolution (defined in board_init.c) */
extern const int LCD_H_RES_VAL;
extern const int LCD_V_RES_VAL;

/* Application-level config, as in esp-board-common. The simulator ignores it. */
typedef struct {
    bool landscape;
} board_app_config_t;

/**
 * Create the SDL2 window and the LVGL display and pointer input.
 * Returns 0 on success.
 *
 * @param app_cfg     Application config. NULL = defaults (portrait).
 * @param disp        Output: LVGL display handle
 * @param touch_indev Output: LVGL pointer input device handle
 */
int board_init(const board_app_config_t *app_cfg,
               lv_display_t **disp, lv_indev_t **touch_indev);

/**
 * SDL event pump and LVGL timer loop. Returns when the window closes.
 */
void board_run(void);

/**
 * Game entry point (main/game_main.c), called from main() in board_init.c.
 */
void game_main(void);

#ifdef __cplusplus
}
#endif
