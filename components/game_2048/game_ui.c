#include "game_ui.h"
#include "game_logic.h"
#include "game_gesture.h"
#include "lv_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "game_ui";

/* ── Layout constants (480x320 landscape) ── */
#define SCREEN_W        480
#define SCREEN_H        320
#define BOARD_SIZE      270
#define CELL_SIZE       60
#define CELL_GAP        6
#define BOARD_PAD       (CELL_GAP)
#define TOP_BAR_H       40
#define BOARD_X         ((SCREEN_W - BOARD_SIZE) / 2)
#define BOARD_Y         (TOP_BAR_H + (SCREEN_H - TOP_BAR_H - BOARD_SIZE) / 2)

/* ── Classic 2048 color palette ── */
typedef struct {
    lv_color_t bg;
    lv_color_t fg;
} tile_style_t;

static lv_color_t color_hex(uint32_t hex)
{
    return lv_color_make((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

static tile_style_t tile_style_for(int value)
{
    tile_style_t s;
    lv_color_t dark_text = color_hex(0x776e65);
    lv_color_t light_text = color_hex(0xf9f6f2);

    switch (value) {
    case 0:    s.bg = color_hex(0xcdc1b4); s.fg = dark_text; break;
    case 2:    s.bg = color_hex(0xeee4da); s.fg = dark_text; break;
    case 4:    s.bg = color_hex(0xede0c8); s.fg = dark_text; break;
    case 8:    s.bg = color_hex(0xf2b179); s.fg = light_text; break;
    case 16:   s.bg = color_hex(0xf59563); s.fg = light_text; break;
    case 32:   s.bg = color_hex(0xf67c5f); s.fg = light_text; break;
    case 64:   s.bg = color_hex(0xf65e3b); s.fg = light_text; break;
    case 128:  s.bg = color_hex(0xedcf72); s.fg = light_text; break;
    case 256:  s.bg = color_hex(0xedcc61); s.fg = light_text; break;
    case 512:  s.bg = color_hex(0xedc850); s.fg = light_text; break;
    case 1024: s.bg = color_hex(0xedc53f); s.fg = light_text; break;
    case 2048: s.bg = color_hex(0xedc22e); s.fg = light_text; break;
    default:   s.bg = color_hex(0x3c3a32); s.fg = light_text; break; /* >2048 */
    }
    return s;
}

static const lv_font_t *font_for_value(int value)
{
    if (value >= 1024) return &lv_font_montserrat_20;
    if (value >= 128)  return &lv_font_montserrat_24;
    return &lv_font_montserrat_28;
}

/* ── Global state ── */
static game_t game;
static lv_obj_t *cells[GRID_SIZE][GRID_SIZE];
static lv_obj_t *cell_labels[GRID_SIZE][GRID_SIZE];
static lv_obj_t *score_value_label;
static lv_obj_t *best_value_label;
static lv_obj_t *board_obj;
static lv_obj_t *overlay_obj = NULL;

/* ── NVS persistence ── */
static void load_best_score(void)
{
    nvs_handle_t handle;
    if (nvs_open("game2048", NVS_READONLY, &handle) == ESP_OK) {
        int32_t val = 0;
        if (nvs_get_i32(handle, "best", &val) == ESP_OK) {
            game.best_score = (int)val;
        }
        nvs_close(handle);
    }
}

static void save_best_score(void)
{
    nvs_handle_t handle;
    if (nvs_open("game2048", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i32(handle, "best", (int32_t)game.best_score);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

/* ── Forward declarations ── */
static void new_game_cb(lv_event_t *e);
static void remove_overlay(void);
static void show_overlay(const char *message, bool show_keep_playing);

/* ── Animations ── */
static void pop_in_anim(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 0, 256);
    lv_anim_set_time(&a, 100);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_zoom);
    lv_anim_start(&a);
}

static void merge_pulse_step1(void *obj, int32_t v)
{
    lv_img_set_zoom((lv_obj_t *)obj, (uint16_t)v);
}

static void merge_pulse_restore(lv_anim_t *a)
{
    lv_img_set_zoom((lv_obj_t *)a->var, 256);
}

static void merge_pulse_anim(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 256, 282);  /* 110% = 256*1.1 ≈ 282 */
    lv_anim_set_time(&a, 75);
    lv_anim_set_playback_time(&a, 75);
    lv_anim_set_exec_cb(&a, merge_pulse_step1);
    lv_anim_set_ready_cb(&a, merge_pulse_restore);
    lv_anim_start(&a);
}

/* ── Track which cells were empty before a move (for pop-in animation) ── */
static int prev_grid[GRID_SIZE][GRID_SIZE];

static void snapshot_grid(void)
{
    memcpy(prev_grid, game.grid, sizeof(prev_grid));
}

/* ── UI update ── */
void game_ui_update(void)
{
    char buf[16];

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            int val = game.grid[r][c];
            tile_style_t ts = tile_style_for(val);

            lv_obj_set_style_bg_color(cells[r][c], ts.bg, 0);

            if (val == 0) {
                lv_label_set_text(cell_labels[r][c], "");
            } else {
                snprintf(buf, sizeof(buf), "%d", val);
                lv_label_set_text(cell_labels[r][c], buf);
                lv_obj_set_style_text_color(cell_labels[r][c], ts.fg, 0);
                lv_obj_set_style_text_font(cell_labels[r][c], font_for_value(val), 0);
            }
            lv_obj_center(cell_labels[r][c]);

            /* Pop-in animation for newly appeared tiles */
            if (prev_grid[r][c] == 0 && val != 0 && !game.merged[r][c]) {
                pop_in_anim(cells[r][c]);
            }
            /* Merge pulse */
            if (game.merged[r][c]) {
                merge_pulse_anim(cells[r][c]);
            }
        }
    }

    snprintf(buf, sizeof(buf), "%d", game.score);
    lv_label_set_text(score_value_label, buf);

    snprintf(buf, sizeof(buf), "%d", game.best_score);
    lv_label_set_text(best_value_label, buf);
}

/* ── Gesture callback (called from game_gesture.c) ── */
void game_on_swipe(direction_t dir)
{
    if (game.state == STATE_LOST) return;
    if (game.state == STATE_WON && !game.keep_playing) return;

    snapshot_grid();

    if (game_move(&game, dir)) {
        game_add_random_tile(&game);
        game_ui_update();
        game_check_state(&game);

        save_best_score();

        if (game.state == STATE_WON && !game.keep_playing) {
            show_overlay("You Win!", true);
        } else if (game.state == STATE_LOST) {
            show_overlay("Game Over!", false);
        }
    }
}

/* ── Overlay (win/lose) ── */
static void keep_playing_cb(lv_event_t *e)
{
    (void)e;
    game.keep_playing = true;
    game.state = STATE_PLAYING;
    remove_overlay();
}

static void try_again_overlay_cb(lv_event_t *e)
{
    (void)e;
    remove_overlay();
    game_reset(&game);
    memset(prev_grid, 0, sizeof(prev_grid));
    game_ui_update();
}

static void remove_overlay(void)
{
    if (overlay_obj) {
        lv_obj_del(overlay_obj);
        overlay_obj = NULL;
    }
}

static lv_obj_t *create_overlay_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 120, 36);
    lv_obj_set_style_bg_color(btn, color_hex(0x8f7a66), 0);
    lv_obj_set_style_radius(btn, 4, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color_hex(0xf9f6f2), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static void show_overlay(const char *message, bool show_keep_playing)
{
    remove_overlay();

    overlay_obj = lv_obj_create(board_obj);
    lv_obj_set_size(overlay_obj, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_pos(overlay_obj, 0, 0);
    lv_obj_set_style_bg_color(overlay_obj, color_hex(0xeee4da), 0);
    lv_obj_set_style_bg_opa(overlay_obj, LV_OPA_80, 0);
    lv_obj_set_style_radius(overlay_obj, 6, 0);
    lv_obj_set_style_border_width(overlay_obj, 0, 0);
    lv_obj_set_style_pad_all(overlay_obj, 0, 0);
    lv_obj_clear_flag(overlay_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(overlay_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay_obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(overlay_obj, 12, 0);

    lv_obj_t *msg_label = lv_label_create(overlay_obj);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(msg_label, color_hex(0x776e65), 0);

    if (show_keep_playing) {
        create_overlay_btn(overlay_obj, "Keep Playing", keep_playing_cb);
    }
    create_overlay_btn(overlay_obj, "Try Again", try_again_overlay_cb);
}

/* ── New Game button callback ── */
static void new_game_cb(lv_event_t *e)
{
    (void)e;
    remove_overlay();
    game_reset(&game);
    memset(prev_grid, 0, sizeof(prev_grid));
    game_ui_update();
}

/* ── Score box helper ── */
static lv_obj_t *create_score_box(lv_obj_t *parent, const char *title, lv_obj_t **value_label)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 70, TOP_BAR_H - 4);
    lv_obj_set_style_bg_color(box, color_hex(0xbbada0), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 2, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(box);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, color_hex(0xeee4da), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    *value_label = lv_label_create(box);
    lv_label_set_text(*value_label, "0");
    lv_obj_set_style_text_color(*value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_14, 0);
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    return box;
}

/* ── Main UI init ── */
void game_ui_init(lv_indev_t *touch_indev)
{
    /* Initialize NVS for best score persistence */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize game state */
    game_init(&game);
    load_best_score();
    memset(prev_grid, 0, sizeof(prev_grid));

    /* Screen background */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, color_hex(0xfaf8ef), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Top bar ── */
    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "2048");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(title, color_hex(0x776e65), 0);
    lv_obj_set_pos(title, 10, 4);

    /* Score boxes */
    lv_obj_t *score_box = create_score_box(scr, "SCORE", &score_value_label);
    lv_obj_set_pos(score_box, SCREEN_W - 240, 2);

    lv_obj_t *best_box = create_score_box(scr, "BEST", &best_value_label);
    lv_obj_set_pos(best_box, SCREEN_W - 164, 2);

    /* New Game button */
    lv_obj_t *new_btn = lv_btn_create(scr);
    lv_obj_set_size(new_btn, 80, 28);
    lv_obj_set_pos(new_btn, SCREEN_W - 80 - 6, 8);
    lv_obj_set_style_bg_color(new_btn, color_hex(0x8f7a66), 0);
    lv_obj_set_style_radius(new_btn, 4, 0);

    lv_obj_t *new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, "New Game");
    lv_obj_set_style_text_color(new_lbl, color_hex(0xf9f6f2), 0);
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, new_game_cb, LV_EVENT_CLICKED, NULL);

    /* ── Board ── */
    board_obj = lv_obj_create(scr);
    lv_obj_set_size(board_obj, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_pos(board_obj, BOARD_X, BOARD_Y);
    lv_obj_set_style_bg_color(board_obj, color_hex(0xbbada0), 0);
    lv_obj_set_style_radius(board_obj, 6, 0);
    lv_obj_set_style_border_width(board_obj, 0, 0);
    lv_obj_set_style_pad_all(board_obj, 0, 0);
    lv_obj_clear_flag(board_obj, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Cells ── */
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            lv_obj_t *cell = lv_obj_create(board_obj);
            int x = CELL_GAP + c * (CELL_SIZE + CELL_GAP);
            int y = CELL_GAP + r * (CELL_SIZE + CELL_GAP);
            lv_obj_set_pos(cell, x, y);
            lv_obj_set_size(cell, CELL_SIZE, CELL_SIZE);
            lv_obj_set_style_radius(cell, 3, 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(cell, color_hex(0xcdc1b4), 0);

            /* Enable transform for zoom animations */
            lv_obj_set_style_transform_zoom(cell, 256, 0);

            lv_obj_t *label = lv_label_create(cell);
            lv_label_set_text(label, "");
            lv_obj_center(label);

            cells[r][c] = cell;
            cell_labels[r][c] = label;
        }
    }

    /* ── Gesture detection ── */
    game_gesture_init(board_obj, touch_indev);

    /* Initial render */
    game_ui_update();

    ESP_LOGI(TAG, "2048 UI initialized");
}
