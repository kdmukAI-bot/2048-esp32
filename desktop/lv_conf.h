/**
 * LVGL configuration for desktop simulator build.
 * Matches the ESP32 sdkconfig.defaults settings.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 16-bit (RGB565) to match ESP32 display */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0  /* No swap needed for SDL2 */

/* Memory */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

/* Display */
#define LV_DPI_DEF 130

/* Fonts — match ESP32 config */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Performance monitor */
#define LV_USE_PERF_MONITOR 0

/* Text */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Widgets */
#define LV_USE_ANIMIMG 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 1
#define LV_USE_COLORWHEEL 0
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LABEL 1
#define LV_USE_LED 0
#define LV_USE_LINE 1
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_OBJID_BUILTIN 0
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 1
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 1
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 1

/* Logging */
#define LV_USE_LOG 0

/* Assert */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_OBJ 0
#define LV_USE_ASSERT_STYLE 0

/* Flex and grid layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* SDL2 driver */
#define LV_USE_SDL 0  /* We'll init SDL manually */

#endif /* LV_CONF_H */
