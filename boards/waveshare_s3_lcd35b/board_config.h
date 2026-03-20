/**
 * Board configuration: Waveshare ESP32-S3 Touch LCD 3.5B
 *
 * Display:  AXS15231B QSPI (320x480)
 * Touch:    AXS15231B I2C (integrated in display controller)
 * PMIC:     AXP2101
 * IO:       TCA9554 I2C GPIO expander (display reset)
 */
#pragma once

#include "driver/gpio.h"

#define BOARD_NAME              "Waveshare ESP32-S3 Touch LCD 3.5B"

/* ── Display ── */
#define BOARD_DISPLAY_DRIVER    DISPLAY_AXS15231B
#define BOARD_LCD_H_RES         320
#define BOARD_LCD_V_RES         480
#define BOARD_PIN_LCD_SCLK      GPIO_NUM_5
#define BOARD_PIN_LCD_DATA0     GPIO_NUM_1
#define BOARD_PIN_LCD_DATA1     GPIO_NUM_2
#define BOARD_PIN_LCD_DATA2     GPIO_NUM_3
#define BOARD_PIN_LCD_DATA3     GPIO_NUM_4
#define BOARD_PIN_LCD_CS        GPIO_NUM_12
#define BOARD_PIN_LCD_RST       GPIO_NUM_NC
#define BOARD_PIN_LCD_BL        GPIO_NUM_6
#define BOARD_SPI_HOST          SPI2_HOST
#define BOARD_LCD_PIXEL_CLOCK   (40 * 1000 * 1000)

/* ── Display quirks ── */
#define BOARD_DISPLAY_QSPI              1   /* Uses QSPI (4 data lines) */
#define BOARD_DISPLAY_QUIRK_RASET_BUG   1   /* CASET/RASET broken over QSPI */
#define BOARD_DISPLAY_DIRECT_MODE       1   /* LVGL direct mode for RASET workaround */

/* ── IO Expander (for display reset) ── */
#define BOARD_HAS_IO_EXPANDER   1
#define BOARD_IO_EXPANDER_ADDR  ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000
#define BOARD_IO_EXPANDER_RST_PIN  IO_EXPANDER_PIN_NUM_1

/* ── Touch ── */
#define BOARD_TOUCH_DRIVER      TOUCH_AXS15231B

/* ── I2C ── */
#define BOARD_PIN_I2C_SDA       GPIO_NUM_8
#define BOARD_PIN_I2C_SCL       GPIO_NUM_7
#define BOARD_I2C_PORT          0

/* ── PMIC ── */
#define BOARD_HAS_PMIC          1
#define BOARD_PMIC_DRIVER       PMIC_AXP2101

/* ── LVGL port tuning ── */
#define BOARD_LVGL_TASK_PRIORITY    5
#define BOARD_LVGL_TASK_STACK       (1024 * 12)
#define BOARD_LVGL_TASK_AFFINITY    1
#define BOARD_LVGL_MAX_SLEEP_MS     500
#define BOARD_LVGL_TIMER_PERIOD_MS  5

/* ── DMA band size for RASET workaround ── */
#define BOARD_LINES_PER_BAND    80
