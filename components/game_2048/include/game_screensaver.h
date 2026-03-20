#ifndef GAME_SCREENSAVER_H
#define GAME_SCREENSAVER_H

#include "lvgl.h"

/**
 * Initialize the screensaver (creates objects but stays hidden).
 * Call after game_ui_init().
 */
void game_screensaver_init(void);

/** Show the screensaver (hides the game UI underneath). */
void game_screensaver_start(void);

/** Hide the screensaver and return to the game. */
void game_screensaver_stop(void);

/** Returns true if the screensaver is currently active. */
bool game_screensaver_is_active(void);

/**
 * Reset the idle timer. Call on any user input.
 * After SCREENSAVER_IDLE_MS with no reset, the screensaver activates.
 */
void game_screensaver_reset_idle(void);

#endif /* GAME_SCREENSAVER_H */
