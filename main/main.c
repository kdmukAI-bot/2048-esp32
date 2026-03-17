#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_axp2101.h"
#include "lv_port.h"
#include "lvgl.h"

#include "esp_io_expander_tca9554.h"

#include "game_logic.h"
#include "game_ui.h"
#include "game_gesture.h"

static const char *TAG = "2048";

#define LCD_H_RES 320
#define LCD_V_RES 480
#define LCD_BUFFER_SIZE (LCD_H_RES * LCD_V_RES)
#define DISPLAY_ROTATION LV_DISP_ROT_NONE

static esp_io_expander_handle_t expander_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    touch_data_t touch_data;

    bsp_touch_read();
    if (bsp_touch_get_coordinates(&touch_data)) {
        last_x = touch_data.coords[0].x;
        last_y = touch_data.coords[0].y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    data->point.x = last_x;
    data->point.y = last_y;
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
    lvgl_port_cfg_t port_cfg = {
        .task_priority = 4,
        .task_stack = 1024 * 8,
        .task_affinity = 1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    lvgl_port_init(&port_cfg);

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_BUFFER_SIZE,
        .trans_size = LCD_BUFFER_SIZE / 10,
        .hres = LCD_H_RES,  /* 320 — native portrait */
        .vres = LCD_V_RES,  /* 480 */
        .sw_rotate = DISPLAY_ROTATION,
        .draw_wait_cb = NULL,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lvgl_touch_indev = lv_indev_drv_register(&indev_drv);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP...");

    /* Step 1: I2C bus */
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_init();

    /* Step 2: IO expander (display reset) */
    io_expander_init(i2c_bus);

    /* Step 3: PMIC */
    bsp_axp2101_init(i2c_bus);

    /* Step 4: Display */
    bsp_display_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);

    /* Step 5: Touch (native portrait coordinates) */
    bsp_touch_init(i2c_bus, LCD_H_RES, LCD_V_RES, DISPLAY_ROTATION);

    /* Step 6: Backlight */
    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

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
