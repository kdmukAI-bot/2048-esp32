#include "game_gesture.h"
#include "game_ui.h"
#include "game_screensaver.h"

#include <stdlib.h>

/* Declared in game_ui.c — the gesture module calls back into UI */
extern void game_on_swipe(direction_t dir);

/* ── Custom gesture detection ──
 *
 * LVGL's built-in gesture recognition requires seeing pointer movement
 * across multiple polling cycles while pressed. Touch controllers with
 * low report rates (like CST816D ~25-60Hz) complete fast swipes in 1-2
 * samples, so LVGL never enters gesture mode.
 *
 * Instead, we track raw press/release coordinates and detect swipes
 * from the total displacement. This catches fast swipes reliably.
 */

/* Minimum displacement (px) to count as a swipe, not a tap */
#define SWIPE_MIN_PX  15

static bool touch_was_pressed = false;
static int32_t press_x, press_y;
static lv_indev_t *tracked_indev = NULL;

static void check_swipe(int32_t dx, int32_t dy)
{
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);

    if (adx < SWIPE_MIN_PX && ady < SWIPE_MIN_PX) return;

    direction_t dir;
    if (adx > ady) {
        dir = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
    } else {
        dir = (dy > 0) ? DIR_DOWN : DIR_UP;
    }

    game_on_swipe(dir);
}

static void touch_poll_cb(lv_timer_t *timer)
{
    if (!tracked_indev) return;

    lv_point_t p;
    lv_indev_get_point(tracked_indev, &p);
    lv_indev_state_t state = lv_indev_get_state(tracked_indev);

    if (state == LV_INDEV_STATE_PRESSED) {
        if (!touch_was_pressed) {
            press_x = p.x;
            press_y = p.y;
            touch_was_pressed = true;

            /* Any touch resets idle timer / wakes screensaver */
            if (game_screensaver_is_active()) {
                game_screensaver_stop();
            } else {
                game_screensaver_reset_idle();
            }
        }
    } else {
        if (touch_was_pressed) {
            /* Only process swipes when screensaver is not active */
            if (!game_screensaver_is_active()) {
                check_swipe(p.x - press_x, p.y - press_y);
            }
            touch_was_pressed = false;
        }
    }
}

void game_gesture_init(lv_obj_t *board, lv_indev_t *touch_indev)
{
    tracked_indev = touch_indev;

    /* Poll raw touch data — custom gesture detection bypasses LVGL gestures */
    lv_timer_create(touch_poll_cb, 10, NULL);
}
