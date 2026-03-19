#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "hw_i2c.h"
#include "hw_display.h"
#include "hw_pmic.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_axs15231b.h"
#include "lvgl.h"

#include "esp_io_expander_tca9554.h"

#include "game_logic.h"
#include "game_ui.h"
#include "game_gesture.h"

static const char *TAG = "2048";

#define LCD_H_RES 320
#define LCD_V_RES 480
#define LCD_BUFFER_SIZE (LCD_H_RES * LCD_V_RES)

/* Render in bands of this many rows to avoid watchdog timeout */
#define LINES_PER_BAND 80

static esp_io_expander_handle_t expander_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* Semaphore to wait for each band's DMA transfer to complete */
static SemaphoreHandle_t flush_done_sem = NULL;

/* Called by SPI DMA when a band transfer completes */
static bool flush_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                           esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(flush_done_sem, &woken);
    return (woken == pdTRUE);
}

/*
 * Custom flush callback for AXS15231B QSPI displays.
 *
 * The AXS15231B ignores CASET/RASET over QSPI — every draw_bitmap starts
 * from (0,0). We must always send the complete framebuffer.
 *
 * We send in bands with semaphore waits between them so the DMA from one
 * band completes before the next starts.
 *
 * Byte swap (RGB565 big-endian for the display) is done per-band into
 * DMA-capable SRAM bounce buffers. Two buffers are used in ping-pong
 * fashion: while DMA sends band N from buf_A, we memcpy+swap band N+1
 * into buf_B. This overlaps CPU work with DMA transfer.
 */
static uint8_t *swap_buf[2] = {NULL, NULL};  /* Double-buffered DMA bounce */

static void axs15231b_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* In direct_mode, LVGL calls flush once per dirty area. We only need to
     * send the full framebuffer on the last flush of the refresh cycle. */
    if (!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }

    const int bpp = 2;  /* RGB565 */
    int buf_idx = 0;

    for (int y = 0; y < LCD_V_RES; y += LINES_PER_BAND) {
        int band_h = (y + LINES_PER_BAND > LCD_V_RES) ? LCD_V_RES - y : LINES_PER_BAND;
        int band_bytes = LCD_H_RES * band_h * bpp;
        uint8_t *src = px_map + (y * LCD_H_RES * bpp);
        uint8_t *dst = swap_buf[buf_idx];

        /* Copy band to SRAM bounce buffer and swap bytes there.
         * This keeps the SPIRAM framebuffer unmodified for direct_mode. */
        memcpy(dst, src, band_bytes);
        lv_draw_sw_rgb565_swap(dst, LCD_H_RES * band_h);

        /* Wait for previous band's DMA before we submit this one */
        if (y > 0) {
            xSemaphoreTake(flush_done_sem, portMAX_DELAY);
        }

        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + band_h, dst);

        /* Swap to the other bounce buffer so we can prepare the next band
         * while this one is in DMA flight */
        buf_idx ^= 1;
    }

    /* Wait for the final band's DMA to complete */
    xSemaphoreTake(flush_done_sem, portMAX_DELAY);

    /* All bands sent and DMA complete — safe for LVGL to modify the buffer */
    lv_display_flush_ready(disp);
}

static void io_expander_init(i2c_master_bus_handle_t bus_handle)
{
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander_handle));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void lvgl_port_setup(void)
{
    /* Create semaphore and double DMA bounce buffers before registering callbacks */
    flush_done_sem = xSemaphoreCreateBinary();
    swap_buf[0] = heap_caps_malloc(LCD_H_RES * LINES_PER_BAND * 2, MALLOC_CAP_DMA);
    swap_buf[1] = heap_caps_malloc(LCD_H_RES * LINES_PER_BAND * 2, MALLOC_CAP_DMA);
    assert(swap_buf[0] != NULL && swap_buf[1] != NULL);

    lvgl_port_cfg_t port_cfg = {
        .task_priority = 5,
        .task_stack = 1024 * 12,
        .task_affinity = 1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    lvgl_port_init(&port_cfg);

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_BUFFER_SIZE,
        .trans_size = 0,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_spiram = true,
            .direct_mode = true,
        },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Override the flush callback to send full screen in bands (RASET bug).
     * Also override the SPI panel IO callback so our semaphore gets signaled
     * instead of esp_lvgl_port's default (which would call lv_disp_flush_ready
     * after every band, confusing LVGL). */
    lv_display_set_flush_cb(lvgl_disp, axs15231b_flush_cb);

    const esp_lcd_panel_io_callbacks_t io_cbs = {
        .on_color_trans_done = flush_ready_cb,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &io_cbs, lvgl_disp);

    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP...");

    /* Step 1: I2C bus */
    i2c_master_bus_handle_t i2c_bus = hw_i2c_init();

    /* Step 2: IO expander (display reset) */
    io_expander_init(i2c_bus);

    /* Step 3: PMIC */
    hw_pmic_init(i2c_bus);

    /* Step 4: Display */
    hw_display_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);

    /* Step 5: Touch (standard esp_lcd_touch driver) */
    esp_lcd_panel_io_handle_t touch_io_handle;
    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
    touch_io_config.scl_speed_hz = 400000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &touch_io_config, &touch_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_axs15231b(touch_io_handle, &tp_cfg, &touch_handle));

    /* Step 6: Backlight */
    hw_display_brightness_init();
    hw_display_set_brightness(100);

    /* Step 7: LVGL port */
    lvgl_port_setup();

    ESP_LOGI(TAG, "BSP initialized. Starting 2048 game...");

    /* Create game UI (acquires LVGL lock internally) */
    if (lvgl_port_lock(0)) {
        game_ui_init(lvgl_touch_indev);
        lvgl_port_unlock();
    }

    /* Main loop — LVGL task runs on its own FreeRTOS task,
       app_main can idle or handle other events */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
