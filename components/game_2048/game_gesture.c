#include "game_gesture.h"
#include "game_ui.h"
#include "esp_log.h"

static const char *TAG = "game_gesture";

/* Declared in game_ui.c — the gesture module calls back into UI */
extern void game_on_swipe(direction_t dir);

static void gesture_event_cb(lv_event_t *e)
{
    lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    direction_t dir;
    switch (gesture_dir) {
    case LV_DIR_LEFT:  dir = DIR_LEFT;  break;
    case LV_DIR_RIGHT: dir = DIR_RIGHT; break;
    case LV_DIR_TOP:   dir = DIR_UP;    break;
    case LV_DIR_BOTTOM:dir = DIR_DOWN;  break;
    default:
        return; /* Ignore diagonal or none */
    }

    ESP_LOGD(TAG, "Swipe: %d", dir);
    game_on_swipe(dir);
}

void game_gesture_init(lv_obj_t *board, lv_indev_t *touch_indev)
{
    (void)touch_indev;

    /* Make the board capture gestures from the full screen for better UX */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_event_cb(scr, gesture_event_cb, LV_EVENT_GESTURE, NULL);
}
