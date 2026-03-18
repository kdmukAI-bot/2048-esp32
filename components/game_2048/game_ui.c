#include "game_ui.h"
#include "game_logic.h"
#include "game_gesture.h"

#include <stdio.h>
#include <string.h>

#ifndef DESKTOP_BUILD
#include "lv_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#else
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

static const char *TAG = "game_ui";

/* ── Layout constants (320x480 portrait) ── */
#define SCREEN_W        320
#define SCREEN_H        480
#define BOARD_SIZE      302   /* 4×63 + 5×10 = 302 */
#define CELL_SIZE       63
#define CELL_GAP        10
#define BOARD_PAD       (CELL_GAP)
#define TOP_BAR_H       95
#define BOARD_X         ((SCREEN_W - BOARD_SIZE) / 2)
#define BOARD_Y         (TOP_BAR_H + 10)
#define BOARD_RADIUS    10
#define CELL_RADIUS     6

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

/* ── Clear Sans fonts (generated from Intel Clear Sans TTF) ── */
LV_FONT_DECLARE(font_clear_sans_bold_20);
LV_FONT_DECLARE(font_clear_sans_bold_24);
LV_FONT_DECLARE(font_clear_sans_bold_28);
LV_FONT_DECLARE(font_clear_sans_bold_30);
LV_FONT_DECLARE(font_clear_sans_bold_40);
LV_FONT_DECLARE(font_clear_sans_bold_50);
LV_FONT_DECLARE(font_clear_sans_regular_14);

static const lv_font_t *font_for_value(int value)
{
    if (value >= 1024) return &font_clear_sans_bold_20;
    if (value >= 128)  return &font_clear_sans_bold_24;
    return &font_clear_sans_bold_28;
}

/* ── Global state ── */
static game_t game;
static lv_obj_t *cells[GRID_SIZE][GRID_SIZE];
static lv_obj_t *cell_labels[GRID_SIZE][GRID_SIZE];
static lv_obj_t *score_value_label;
static lv_obj_t *best_value_label;
static lv_obj_t *board_obj;
static lv_obj_t *overlay_obj = NULL;

/* ── NVS persistence (ESP32 only) ── */
#ifndef DESKTOP_BUILD
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
#else
static void load_best_score(void) { /* no-op on desktop */ }
static void save_best_score(void) { /* no-op on desktop */ }
#endif

/* ── Forward declarations ── */
static void new_game_cb(lv_event_t *e);
static void remove_overlay(void);
static void show_overlay(const char *message, bool show_keep_playing);
static void update_cell_content(int r, int c);


/* ── Track which cells were empty before a move (for pop-in animation) ── */
static int prev_grid[GRID_SIZE][GRID_SIZE];

static void snapshot_grid(void)
{
    memcpy(prev_grid, game.grid, sizeof(prev_grid));
}

/* ── Slide animation state ── */
static bool animating = false;
static int anim_pending = 0;
#define MAX_SLIDE_FILLS (GRID_SIZE * GRID_SIZE)
static lv_obj_t *slide_fills[MAX_SLIDE_FILLS];
static int slide_fill_count = 0;

static int cell_x(int c) { return CELL_GAP + c * (CELL_SIZE + CELL_GAP); }
static int cell_y(int r) { return CELL_GAP + r * (CELL_SIZE + CELL_GAP); }

static void slide_x_cb(void *obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)v);
}

static void slide_y_cb(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)v);
}

static void slide_complete(void)
{
    /* Delete temporary fill objects */
    for (int i = 0; i < slide_fill_count; i++) {
        lv_obj_del(slide_fills[i]);
    }
    slide_fill_count = 0;

    /* Snap all cells back to canonical positions */
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            lv_obj_set_pos(cells[r][c], cell_x(c), cell_y(r));
        }
    }

    /* Update visuals to final state (triggers pop-in and merge pulse) */
    game_ui_update();
    animating = false;

    game_check_state(&game);
    save_best_score();

    if (game.state == STATE_WON && !game.keep_playing) {
        show_overlay("You Win!", true);
    } else if (game.state == STATE_LOST) {
        show_overlay("Game Over!", false);
    }
}

static void slide_anim_done_cb(lv_anim_t *a)
{
    (void)a;
    if (--anim_pending <= 0) {
        slide_complete();
    }
}

/* ── UI update ── */
static void update_cell_content(int r, int c)
{
    char buf[16];
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
}

static void update_scores(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", game.score);
    lv_label_set_text(score_value_label, buf);

    snprintf(buf, sizeof(buf), "%d", game.best_score);
    lv_label_set_text(best_value_label, buf);
}

void game_ui_update(void)
{
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            update_cell_content(r, c);

            /* Merge pulse skipped — zoom-based animations cause rendering
             * artifacts at 16 FPS due to LVGL transform layer overhead */
        }
    }

    update_scores();
}

/* ── Gesture callback (called from game_gesture.c) ── */
void game_on_swipe(direction_t dir)
{
    if (animating) return;
    if (game.state == STATE_LOST) return;
    if (game.state == STATE_WON && !game.keep_playing) return;

    snapshot_grid();

    if (!game_move(&game, dir)) return;

    game_add_random_tile(&game);

    /* Count how many tiles actually move on screen */
    int move_count = 0;
    for (int i = 0; i < game.last_moves.count; i++) {
        tile_move_t *m = &game.last_moves.moves[i];
        if (m->from_r != m->to_r || m->from_c != m->to_c)
            move_count++;
    }

    if (move_count == 0) {
        game_ui_update();
        game_check_state(&game);
        save_best_score();
        if (game.state == STATE_WON && !game.keep_playing)
            show_overlay("You Win!", true);
        else if (game.state == STATE_LOST)
            show_overlay("Game Over!", false);
        return;
    }

    /* Animate source cells sliding to destination positions.
     * Cells still show pre-move content during the slide. */
    animating = true;
    anim_pending = move_count;
    slide_fill_count = 0;

    for (int i = 0; i < game.last_moves.count; i++) {
        tile_move_t *m = &game.last_moves.moves[i];
        if (m->from_r == m->to_r && m->from_c == m->to_c) continue;

        /* Create a temporary empty-tile-colored fill at the source position
         * so the board background doesn't flash through when the cell leaves */
        if (slide_fill_count < MAX_SLIDE_FILLS) {
            lv_obj_t *fill = lv_obj_create(board_obj);
            lv_obj_set_pos(fill, cell_x(m->from_c), cell_y(m->from_r));
            lv_obj_set_size(fill, CELL_SIZE, CELL_SIZE);
            lv_obj_set_style_radius(fill, CELL_RADIUS, 0);
            lv_obj_set_style_border_width(fill, 0, 0);
            lv_obj_set_style_bg_color(fill, color_hex(0xcdc1b4), 0);
            lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
            slide_fills[slide_fill_count++] = fill;
        }

        lv_obj_t *cell = cells[m->from_r][m->from_c];

        /* Bring sliding tile to front so it renders on top */
        lv_obj_move_foreground(cell);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, cell);
        lv_anim_set_time(&a, 120);  /* ~2 frames at 16 FPS */
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_ready_cb(&a, slide_anim_done_cb);

        if (m->from_c != m->to_c) {
            lv_anim_set_values(&a, cell_x(m->from_c), cell_x(m->to_c));
            lv_anim_set_exec_cb(&a, slide_x_cb);
        } else {
            lv_anim_set_values(&a, cell_y(m->from_r), cell_y(m->to_r));
            lv_anim_set_exec_cb(&a, slide_y_cb);
        }

        lv_anim_start(&a);
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
    lv_obj_set_style_text_font(lbl, &font_clear_sans_regular_14, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static void show_overlay(const char *message, bool show_keep_playing)
{
    remove_overlay();

    /* Win = gold overlay, lose = beige overlay (matching original 2048) */
    lv_color_t overlay_color = show_keep_playing ? color_hex(0xedc22e) : color_hex(0xeee4da);

    overlay_obj = lv_obj_create(board_obj);
    lv_obj_set_size(overlay_obj, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_pos(overlay_obj, 0, 0);
    lv_obj_set_style_bg_color(overlay_obj, overlay_color, 0);
    lv_obj_set_style_bg_opa(overlay_obj, LV_OPA_50, 0);
    lv_obj_set_style_radius(overlay_obj, BOARD_RADIUS, 0);
    lv_obj_set_style_border_width(overlay_obj, 0, 0);
    lv_obj_set_style_pad_all(overlay_obj, 0, 0);
    lv_obj_clear_flag(overlay_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(overlay_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay_obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(overlay_obj, 12, 0);

    lv_obj_t *msg_label = lv_label_create(overlay_obj);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &font_clear_sans_bold_30, 0);
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
    lv_obj_set_size(box, 70, 32);  /* Default size; caller may override */
    lv_obj_set_style_bg_color(box, color_hex(0xbbada0), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 2, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(box);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, color_hex(0xeee4da), 0);
    lv_obj_set_style_text_font(title_lbl, &font_clear_sans_regular_14, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    *value_label = lv_label_create(box);
    lv_label_set_text(*value_label, "0");
    lv_obj_set_style_text_color(*value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(*value_label, &font_clear_sans_regular_14, 0);
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    return box;
}

/* ── Main UI init ── */
void game_ui_init(lv_indev_t *touch_indev)
{
#ifndef DESKTOP_BUILD
    /* Initialize NVS for best score persistence */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#endif

    /* Initialize game state */
    game_init(&game);
    load_best_score();
    memset(prev_grid, 0, sizeof(prev_grid));

    /* Screen background */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, color_hex(0xfaf8ef), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Top bar (matches original 2048 app layout) ── */
    /* "2048" title on the left, score boxes to the right */
    int score_box_w = 72;
    int score_box_h = 38;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "2048");
    lv_obj_set_style_text_font(title, &font_clear_sans_bold_50, 0);
    lv_obj_set_style_text_color(title, color_hex(0x776e65), 0);
    lv_obj_set_pos(title, 10, 16);

    lv_obj_t *score_box = create_score_box(scr, "SCORE", &score_value_label);
    lv_obj_set_size(score_box, score_box_w, score_box_h);
    lv_obj_set_pos(score_box, SCREEN_W - score_box_w * 2 - 6 - 10, 14);

    lv_obj_t *best_box = create_score_box(scr, "BEST", &best_value_label);
    lv_obj_set_size(best_box, score_box_w, score_box_h);
    lv_obj_set_pos(best_box, SCREEN_W - score_box_w - 10, 14);

    /* Refresh (new game) icon button — right-aligned below scores */
    int btn_size = 36;
    lv_obj_t *new_btn = lv_btn_create(scr);
    lv_obj_set_size(new_btn, btn_size, btn_size);
    lv_obj_set_pos(new_btn, SCREEN_W - btn_size - 10, 56);
    lv_obj_set_style_bg_color(new_btn, color_hex(0xbbada0), 0);
    lv_obj_set_style_radius(new_btn, BOARD_RADIUS, 0);

    lv_obj_t *new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(new_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, new_game_cb, LV_EVENT_CLICKED, NULL);

    /* ── Board ── */
    board_obj = lv_obj_create(scr);
    lv_obj_set_size(board_obj, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_pos(board_obj, BOARD_X, BOARD_Y);
    lv_obj_set_style_bg_color(board_obj, color_hex(0xbbada0), 0);
    lv_obj_set_style_radius(board_obj, BOARD_RADIUS, 0);
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
            lv_obj_set_style_radius(cell, CELL_RADIUS, 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(cell, color_hex(0xcdc1b4), 0);

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
