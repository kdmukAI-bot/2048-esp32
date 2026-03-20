/**
 * 2048 Bouncing Tile Screensaver
 *
 * A tile bounces around the screen like the classic DVD logo.
 * Each wall bounce doubles the tile value and changes its color
 * through the 2048 palette (2→4→8→...→2048→2).
 *
 * Any touch dismisses the screensaver.
 * Activates after SCREENSAVER_IDLE_MS of no touch input.
 */

#include "game_screensaver.h"
#include "game_logic.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef DESKTOP_BUILD
#include "esp_log.h"
#else
#include <stdio.h>
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

static const char *TAG = "screensaver";

/* ── Configuration ── */
#define SCREENSAVER_IDLE_MS     30000   /* 30 seconds idle → screensaver */
#define ANIM_TICK_MS            30      /* ~33 FPS animation */
#define TILE_SIZE               63      /* Tile size in pixels */
#define TILE_RADIUS             6
#define SPEED_MIN               2       /* Minimum pixels per tick */
#define SPEED_MAX               5       /* Maximum pixels per tick */

/* ── 2048 color palette (duplicated from game_ui.c for encapsulation) ── */
static lv_color_t color_hex(uint32_t hex)
{
    return lv_color_make((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

typedef struct {
    lv_color_t bg;
    lv_color_t fg;
} tile_colors_t;

static tile_colors_t colors_for_value(int value)
{
    tile_colors_t c;
    lv_color_t dark = color_hex(0x776e65);
    lv_color_t light = color_hex(0xf9f6f2);

    switch (value) {
    case 2:    c.bg = color_hex(0xeee4da); c.fg = dark;  break;
    case 4:    c.bg = color_hex(0xede0c8); c.fg = dark;  break;
    case 8:    c.bg = color_hex(0xf2b179); c.fg = light; break;
    case 16:   c.bg = color_hex(0xf59563); c.fg = light; break;
    case 32:   c.bg = color_hex(0xf67c5f); c.fg = light; break;
    case 64:   c.bg = color_hex(0xf65e3b); c.fg = light; break;
    case 128:  c.bg = color_hex(0xedcf72); c.fg = light; break;
    case 256:  c.bg = color_hex(0xedcc61); c.fg = light; break;
    case 512:  c.bg = color_hex(0xedc850); c.fg = light; break;
    case 1024: c.bg = color_hex(0xedc53f); c.fg = light; break;
    case 2048: c.bg = color_hex(0xedc22e); c.fg = light; break;
    default:   c.bg = color_hex(0x3c3a32); c.fg = light; break;
    }
    return c;
}

/* Font selection based on tile value digit count */
LV_FONT_DECLARE(font_clear_sans_bold_20);
LV_FONT_DECLARE(font_clear_sans_bold_24);
LV_FONT_DECLARE(font_clear_sans_bold_28);

static const lv_font_t *font_for_value(int value)
{
    if (value >= 1024) return &font_clear_sans_bold_20;
    if (value >= 128)  return &font_clear_sans_bold_24;
    return &font_clear_sans_bold_28;
}

/* ── State ── */
static bool active = false;
static lv_obj_t *scr_obj = NULL;       /* Full-screen background */
static lv_obj_t *tile_obj = NULL;      /* The bouncing tile */
static lv_obj_t *tile_label = NULL;    /* Number on the tile */
static lv_timer_t *anim_timer = NULL;
static lv_timer_t *idle_timer = NULL;

static int tile_x, tile_y;             /* Current position */
static int dx, dy;                     /* Velocity */
static int tile_value;                 /* Current tile value (2-2048) */
static int screen_w, screen_h;         /* Display dimensions */

static void update_tile_appearance(void)
{
    tile_colors_t c = colors_for_value(tile_value);
    lv_obj_set_style_bg_color(tile_obj, c.bg, 0);
    lv_obj_set_style_text_color(tile_label, c.fg, 0);
    lv_obj_set_style_text_font(tile_label, font_for_value(tile_value), 0);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", tile_value);
    lv_label_set_text(tile_label, buf);
    lv_obj_center(tile_label);
}

static int random_speed(void)
{
    return SPEED_MIN + (rand() % (SPEED_MAX - SPEED_MIN + 1));
}

static void next_value(void)
{
    tile_value *= 2;
    if (tile_value > 2048) tile_value = 2;
    update_tile_appearance();
}

static void anim_tick_cb(lv_timer_t *timer)
{
    if (!active) return;

    tile_x += dx;
    tile_y += dy;

    /* Allow tile to travel a quarter off-screen before bouncing.
     * Randomize speed on each bounce (independently per axis) to
     * prevent repeating loop patterns. */
    int overshoot = TILE_SIZE / 4;
    bool bounced = false;

    if (tile_x <= -overshoot) {
        tile_x = -overshoot;
        dx = random_speed();
        bounced = true;
    } else if (tile_x >= screen_w - TILE_SIZE + overshoot) {
        tile_x = screen_w - TILE_SIZE + overshoot;
        dx = -random_speed();
        bounced = true;
    }

    if (tile_y <= -overshoot) {
        tile_y = -overshoot;
        dy = random_speed();
        bounced = true;
    } else if (tile_y >= screen_h - TILE_SIZE + overshoot) {
        tile_y = screen_h - TILE_SIZE + overshoot;
        dy = -random_speed();
        bounced = true;
    }

    if (bounced) {
        next_value();
    }

    lv_obj_set_pos(tile_obj, tile_x, tile_y);
}

static void idle_timeout_cb(lv_timer_t *timer)
{
    if (!active) {
        ESP_LOGI(TAG, "Idle timeout — starting screensaver");
        game_screensaver_start();
    }
}

/* ── Public API ── */

void game_screensaver_init(void)
{
    lv_display_t *disp = lv_display_get_default();
    screen_w = lv_display_get_horizontal_resolution(disp);
    screen_h = lv_display_get_vertical_resolution(disp);

    /* Create screensaver screen objects (hidden initially) */
    scr_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(scr_obj, screen_w, screen_h);
    lv_obj_set_pos(scr_obj, 0, 0);
    lv_obj_set_style_bg_color(scr_obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(scr_obj, 0, 0);
    lv_obj_set_style_radius(scr_obj, 0, 0);
    lv_obj_set_style_pad_all(scr_obj, 0, 0);
    lv_obj_set_style_clip_corner(scr_obj, true, 0);
    lv_obj_clear_flag(scr_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr_obj, LV_OBJ_FLAG_HIDDEN);

    /* The bouncing tile */
    tile_obj = lv_obj_create(scr_obj);
    lv_obj_set_size(tile_obj, TILE_SIZE, TILE_SIZE);
    lv_obj_set_style_radius(tile_obj, TILE_RADIUS, 0);
    lv_obj_set_style_border_width(tile_obj, 0, 0);
    lv_obj_set_style_pad_all(tile_obj, 0, 0);
    lv_obj_clear_flag(tile_obj, LV_OBJ_FLAG_SCROLLABLE);

    tile_label = lv_label_create(tile_obj);
    lv_obj_center(tile_label);

    /* Animation timer (paused until screensaver starts) */
    anim_timer = lv_timer_create(anim_tick_cb, ANIM_TICK_MS, NULL);
    lv_timer_pause(anim_timer);

    /* Idle timer — fires once after timeout */
    idle_timer = lv_timer_create(idle_timeout_cb, SCREENSAVER_IDLE_MS, NULL);
    lv_timer_set_repeat_count(idle_timer, 1);

    ESP_LOGI(TAG, "Screensaver initialized (idle timeout: %d ms)", SCREENSAVER_IDLE_MS);
}

void game_screensaver_start(void)
{
    if (active) return;
    active = true;

    /* Seed RNG from LVGL tick for variety between runs */
    srand(lv_tick_get());

    /* Reset tile state with random initial direction */
    tile_value = 2;
    tile_x = screen_w / 4 + (rand() % (screen_w / 2));
    tile_y = screen_h / 4 + (rand() % (screen_h / 2));
    dx = (rand() & 1) ? random_speed() : -random_speed();
    dy = (rand() & 1) ? random_speed() : -random_speed();

    update_tile_appearance();
    lv_obj_set_pos(tile_obj, tile_x, tile_y);

    /* Show screensaver overlay */
    lv_obj_clear_flag(scr_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(scr_obj);

    /* Start animation */
    lv_timer_resume(anim_timer);

    ESP_LOGI(TAG, "Screensaver started");
}

void game_screensaver_stop(void)
{
    if (!active) return;
    active = false;

    /* Hide and pause */
    lv_obj_add_flag(scr_obj, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(anim_timer);

    /* Restart idle timer */
    game_screensaver_reset_idle();

    ESP_LOGI(TAG, "Screensaver stopped");
}

bool game_screensaver_is_active(void)
{
    return active;
}

void game_screensaver_reset_idle(void)
{
    if (idle_timer) {
        lv_timer_reset(idle_timer);
        lv_timer_set_repeat_count(idle_timer, 1);
        lv_timer_resume(idle_timer);
    }
}
