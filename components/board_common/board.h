/**
 * Board abstraction interface.
 *
 * Each board is defined by a board_config.h selected at compile time via the
 * BOARD CMake variable.  The generic board_init() in board_init.c dispatches
 * on the config defines to initialise the correct drivers.
 */
#pragma once

#include "lvgl.h"

/* ── Driver selection enums ── */
#define DISPLAY_ST7796      1
#define DISPLAY_ST7789      2
#define DISPLAY_AXS15231B   3

#define TOUCH_FT6336        1
#define TOUCH_CST816D       2
#define TOUCH_AXS15231B     3

#define PMIC_AXP2101        1

/* ── Resolution (set in board_init.c from board_config.h) ── */
extern const int LCD_H_RES_VAL;
extern const int LCD_V_RES_VAL;

/* ── Board interface ── */

/**
 * Initialise all board hardware and the LVGL display/touch port.
 * Returns 0 on success.
 */
int board_init(lv_display_t **disp, lv_indev_t **touch_indev);

/**
 * Board main loop (never returns).
 * ESP32: idle loop with vTaskDelay.
 * Desktop: SDL event pump.
 */
void board_run(void);

/**
 * Shared entry point called by each platform's main/app_main.
 */
void game_main(void);
