/**
 * 2048 Desktop Simulator
 *
 * Runs the same game logic + LVGL UI as the ESP32 build,
 * using SDL2 as the display/input backend.
 *
 * Controls:
 *   Arrow keys / WASD  — swipe directions
 *   Escape             — new game
 *   Mouse click        — LVGL pointer (for New Game button)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "lvgl.h"
#include "game_logic.h"
#include "game_ui.h"
#include "game_gesture.h"

/* Display dimensions (native portrait) */
#define DISP_HOR_RES 320
#define DISP_VER_RES 480
#define SDL_WIN_W DISP_HOR_RES
#define SDL_WIN_H DISP_VER_RES

/* SDL state */
static SDL_Window   *sdl_window   = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture  *sdl_texture  = NULL;

/* LVGL display buffer */
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[DISP_HOR_RES * DISP_VER_RES];

/* Mouse state for LVGL input driver */
static int32_t mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;

/* ── SDL2 display flush callback ── */
static void sdl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    /* Update the full texture from the LVGL buffer */
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    SDL_Rect rect = {
        .x = area->x1,
        .y = area->y1,
        .w = w,
        .h = h,
    };

    /* LVGL uses RGB565 (LV_COLOR_DEPTH=16) */
    SDL_UpdateTexture(sdl_texture, &rect, color_p, w * sizeof(lv_color_t));
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);

    lv_disp_flush_ready(drv);
}

/* ── SDL2 mouse input callback ── */
static void sdl_mouse_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

/* ── Initialize SDL2 + LVGL ── */
static bool init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    sdl_window = SDL_CreateWindow(
        "2048",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SDL_WIN_W, SDL_WIN_H,
        0
    );
    if (!sdl_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    /* RGB565 texture matching LVGL color format */
    sdl_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        DISP_HOR_RES, DISP_VER_RES
    );
    if (!sdl_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

static lv_indev_t *init_lvgl_display(void)
{
    lv_init();

    /* Display buffer — full screen, single buffer */
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, DISP_HOR_RES * DISP_VER_RES);

    /* Display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = sdl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    /* Mouse input driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read_cb;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv);

    return mouse_indev;
}

/* ── Keyboard → game swipe mapping ── */
static void handle_key(SDL_Keycode key)
{
    /* Declared in game_ui.c */
    extern void game_on_swipe(direction_t dir);

    switch (key) {
    case SDLK_UP:
    case SDLK_w:
        game_on_swipe(DIR_UP);
        break;
    case SDLK_DOWN:
    case SDLK_s:
        game_on_swipe(DIR_DOWN);
        break;
    case SDLK_LEFT:
    case SDLK_a:
        game_on_swipe(DIR_LEFT);
        break;
    case SDLK_RIGHT:
    case SDLK_d:
        game_on_swipe(DIR_RIGHT);
        break;
    case SDLK_ESCAPE:
        /* Trigger new game — same as game_ui.c new_game_cb */
        {
            extern void game_ui_update(void);
            static game_t *game_ptr = NULL;
            /* Access game via game_on_swipe side effect — just reset via UI */
            /* We'll trigger ESC as a special swipe that resets */
        }
        break;
    default:
        break;
    }
}

/* ── Main loop ── */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!init_sdl()) {
        return 1;
    }

    lv_indev_t *mouse_indev = init_lvgl_display();

    /* Create game UI — same call as ESP32 app_main */
    game_ui_init(mouse_indev);

    printf("2048 Desktop Simulator running.\n");
    printf("  Arrow keys / WASD: move tiles\n");
    printf("  Click 'New Game' button to restart\n");
    printf("  Close window or Ctrl+C to quit\n");

    bool running = true;
    uint32_t last_tick = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                handle_key(e.key.keysym.sym);
                break;
            case SDL_MOUSEMOTION:
                mouse_x = e.motion.x;
                mouse_y = e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_pressed = true;
                mouse_x = e.button.x;
                mouse_y = e.button.y;
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_pressed = false;
                break;
            }
        }

        /* LVGL tick */
        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_tick;
        if (elapsed > 0) {
            lv_tick_inc(elapsed);
            last_tick = now;
        }

        lv_timer_handler();

        /* ~60 FPS */
        SDL_Delay(5);
    }

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();

    return 0;
}
