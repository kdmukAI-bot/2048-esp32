/**
 * Desktop board — SDL2 display/input backend.
 *
 * Implements the board interface (board.h) for desktop platforms.
 * SDL2 window + LVGL display, mouse pointer input, keyboard → swipe mapping.
 *
 * Controls:
 *   Arrow keys / WASD  — swipe directions
 *   Mouse click        — LVGL pointer (for New Game button)
 *   Close window       — quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "board.h"
#include "game_logic.h"

/* Display dimensions (native portrait, 320x480 default) */
#define DISP_HOR_RES 320
#define DISP_VER_RES 480

/* ── Resolution globals (declared in board.h) ── */
const int LCD_H_RES_VAL = DISP_HOR_RES;
const int LCD_V_RES_VAL = DISP_VER_RES;

/* SDL state */
static SDL_Window   *sdl_window   = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture  *sdl_texture  = NULL;

/* LVGL display buffer */
static uint8_t buf1[DISP_HOR_RES * DISP_VER_RES * 2];

/* Mouse state for LVGL input driver */
static int32_t mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;

/* Flag for event loop */
static bool running = true;

/* ── SDL2 display flush callback ── */
static void sdl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    SDL_Rect rect = {
        .x = area->x1,
        .y = area->y1,
        .w = w,
        .h = h,
    };

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

/* ── Keyboard → game swipe mapping ── */
static void handle_key(SDL_Keycode key)
{
    extern void game_on_swipe(direction_t dir);

    switch (key) {
    case SDLK_UP:    case SDLK_w: game_on_swipe(DIR_UP);    break;
    case SDLK_DOWN:  case SDLK_s: game_on_swipe(DIR_DOWN);  break;
    case SDLK_LEFT:  case SDLK_a: game_on_swipe(DIR_LEFT);  break;
    case SDLK_RIGHT: case SDLK_d: game_on_swipe(DIR_RIGHT); break;
    default: break;
    }
}

/* ── Initialize SDL2 ── */
static bool init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    sdl_window = SDL_CreateWindow(
        "2048",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISP_HOR_RES, DISP_VER_RES, 0);
    if (!sdl_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    sdl_texture = SDL_CreateTexture(
        sdl_renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        DISP_HOR_RES, DISP_VER_RES);
    if (!sdl_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

/* ── Board interface ── */

int board_init(lv_display_t **disp, lv_indev_t **touch_indev)
{
    if (!init_sdl()) {
        exit(1);
    }

    lv_init();

    /* Display */
    lv_display_t *d = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(d, sdl_flush_cb);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf1, NULL,
                           DISP_HOR_RES * DISP_VER_RES * 2,
                           LV_DISPLAY_RENDER_MODE_FULL);
    *disp = d;

    /* Mouse input as pointer device */
    lv_indev_t *mouse = lv_indev_create();
    lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse, sdl_mouse_read_cb);
    *touch_indev = mouse;

    printf("2048 Desktop Simulator running.\n");
    printf("  Arrow keys / WASD: move tiles\n");
    printf("  Click 'New Game' button to restart\n");
    printf("  Close window or Ctrl+C to quit\n");

    return 0;
}

void board_run(void)
{
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

        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_tick;
        if (elapsed > 0) {
            lv_tick_inc(elapsed);
            last_tick = now;
        }

        lv_timer_handler();
        SDL_Delay(5);
    }

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    game_main();
    return 0;
}
