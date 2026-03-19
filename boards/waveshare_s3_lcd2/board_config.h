/**
 * Board configuration: Waveshare ESP32-S3 Touch LCD 2
 *
 * Display:  ST7789 SPI (240x320)
 * Touch:    CST816D I2C (CST816S family)
 * PMIC:     none
 *
 * NOTE: Pin assignments are preliminary — verify against Waveshare
 * schematic before first hardware test.
 */
#pragma once

#include "driver/gpio.h"

#define BOARD_NAME              "Waveshare ESP32-S3 Touch LCD 2"

/* ── Display ── */
#define BOARD_DISPLAY_DRIVER    DISPLAY_ST7789
#define BOARD_LCD_H_RES         240
#define BOARD_LCD_V_RES         320
#define BOARD_PIN_LCD_SCLK      GPIO_NUM_36
#define BOARD_PIN_LCD_MOSI      GPIO_NUM_35
#define BOARD_PIN_LCD_CS        GPIO_NUM_34
#define BOARD_PIN_LCD_DC        GPIO_NUM_33
#define BOARD_PIN_LCD_RST       GPIO_NUM_38
#define BOARD_PIN_LCD_BL        GPIO_NUM_1
#define BOARD_SPI_HOST          SPI2_HOST
#define BOARD_LCD_PIXEL_CLOCK   (40 * 1000 * 1000)

/* ── Display quirks ── */
#define BOARD_DISPLAY_QSPI              0
#define BOARD_DISPLAY_QUIRK_RASET_BUG   0
#define BOARD_DISPLAY_DIRECT_MODE       0

/* ── IO Expander ── */
#define BOARD_HAS_IO_EXPANDER   0

/* ── Touch ── */
#define BOARD_TOUCH_DRIVER      TOUCH_CST816D

/* ── I2C ── */
#define BOARD_PIN_I2C_SDA       GPIO_NUM_39
#define BOARD_PIN_I2C_SCL       GPIO_NUM_40
#define BOARD_I2C_PORT          0

/* ── PMIC ── */
#define BOARD_HAS_PMIC          0

/* ── LVGL port tuning ── */
#define BOARD_LVGL_TASK_PRIORITY    5
#define BOARD_LVGL_TASK_STACK       (1024 * 8)
#define BOARD_LVGL_TASK_AFFINITY    1
#define BOARD_LVGL_MAX_SLEEP_MS     500
#define BOARD_LVGL_TIMER_PERIOD_MS  5
