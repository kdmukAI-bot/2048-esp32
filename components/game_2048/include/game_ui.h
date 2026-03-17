#ifndef GAME_UI_H
#define GAME_UI_H

#include "lvgl.h"
#include "game_logic.h"

void game_ui_init(lv_indev_t *touch_indev);
void game_ui_update(void);

#endif /* GAME_UI_H */
