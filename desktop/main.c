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
static uint8_t buf1[DISP_HOR_RES * DISP_VER_RES * 2];

/* Mouse state for LVGL input driver */
static int32_t mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;

/* ── SDL2 display flush callback ── */
static void sdl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
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
    SDL_UpdateTexture(sdl_texture, &rect, px_map, w * 2);
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);

    lv_display_flush_ready(disp);
}

/* ── SDL2 mouse input callback ── */
static void sdl_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
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

    /* Display — v9 API */
    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, sdl_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, buf1, NULL,
                           DISP_HOR_RES * DISP_VER_RES * 2,
                           LV_DISPLAY_RENDER_MODE_FULL);

    /* Mouse input — v9 API */
    lv_indev_t *mouse_indev = lv_indev_create();
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, sdl_mouse_read_cb);

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
