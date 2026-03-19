# LVGL v8 → v9 Migration Plan + Replace Custom Porting/Touch with Registry Components

Executed on `2048-esp32`. Reusable for `seedsigner-c-modules` and any other ESP32-S3 project on the Waveshare ESP32-S3 Touch LCD 3.5B.

---

## AXS15231B QSPI Display — Critical Hardware Limitation

**The AXS15231B display controller has a confirmed hardware defect: CASET (0x2A) and RASET (0x2B) commands have NO EFFECT over QSPI.** The draw coordinates are ignored — `esp_lcd_panel_draw_bitmap()` always starts from (0,0) regardless of the x/y parameters passed.

**Consequence:** Partial-buffer rendering (LVGL's default mode) produces shredded/corrupted output because dirty regions are drawn at the wrong screen position. You MUST use `full_refresh` mode.

**Sources confirming this:**
- [LVGL Forum: JC3248W535 AXS15231B driver](https://forum.lvgl.io/t/jc3248w535-axs15231b-driver/18707) — "commands 0x2A and 0x2B don't have any effects... only a full display refresh management is possible"
- [LVGL Forum: LVGL 9.2 + AXS15231B](https://forum.lvgl.io/t/lvgl-9-2-esp32-s3-display-axs15321b/19735) — "something goes wrong with partial buffer display rendering"
- [GitHub: lvgl-micropython Discussion #161](https://github.com/lvgl-micropython/lvgl_micropython/discussions/161) — commenting out RASET resolves artifacts
- [GitHub: esp-iot-solution Issue #579](https://github.com/espressif/esp-iot-solution/issues/579) — `draw_bitmap` ignores coordinates

### Display Config Solution (Verified Working)

Use `direct_mode` with a full-screen SPIRAM buffer and a **custom flush callback** that:
1. Always sends the entire framebuffer (works around the RASET bug)
2. Sends in bands with DMA sync (avoids watchdog timeout)
3. Byte-swaps into a DMA-capable SRAM bounce buffer (avoids corrupting the persistent framebuffer)

```c
// Display config — use direct_mode, NOT full_refresh or swap_bytes:
lvgl_port_display_cfg_t disp_cfg = {
    .buffer_size = LCD_H_RES * LCD_V_RES,  // full framebuffer in SPIRAM
    .trans_size = 0,                        // we handle transfer chunking ourselves
    .hres = LCD_H_RES,
    .vres = LCD_V_RES,
    .color_format = LV_COLOR_FORMAT_RGB565,
    .flags = {
        .buff_spiram = true,
        .direct_mode = true,    // persistent framebuffer, only dirty pixels redrawn
        // Do NOT set swap_bytes or full_refresh
    },
};
lvgl_disp = lvgl_port_add_disp(&disp_cfg);

// Override flush callback and SPI DMA callback:
lv_display_set_flush_cb(lvgl_disp, axs15231b_flush_cb);
esp_lcd_panel_io_register_event_callbacks(io_handle, &io_cbs, lvgl_disp);
```

### Custom Flush Callback (Key Implementation)

```c
#define LINES_PER_BAND 80

static SemaphoreHandle_t flush_done_sem;
static uint8_t *swap_buf[2];  /* Double-buffered DMA bounce buffers */

/* SPI DMA completion callback — signals semaphore instead of lv_disp_flush_ready */
static bool flush_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                           esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(flush_done_sem, &woken);
    return (woken == pdTRUE);
}

static void axs15231b_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* In direct_mode, LVGL calls flush once per dirty area. Only send the
     * full framebuffer on the LAST flush of the refresh cycle. */
    if (!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }

    const int bpp = 2;
    int buf_idx = 0;
    for (int y = 0; y < LCD_V_RES; y += LINES_PER_BAND) {
        int band_h = (y + LINES_PER_BAND > LCD_V_RES) ? LCD_V_RES - y : LINES_PER_BAND;
        int band_bytes = LCD_H_RES * band_h * bpp;
        uint8_t *src = px_map + (y * LCD_H_RES * bpp);
        uint8_t *dst = swap_buf[buf_idx];

        memcpy(dst, src, band_bytes);                       // copy to SRAM
        lv_draw_sw_rgb565_swap(dst, LCD_H_RES * band_h);   // swap the copy
        if (y > 0) xSemaphoreTake(flush_done_sem, portMAX_DELAY); // wait prev
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + band_h, dst);
        buf_idx ^= 1;  // ping-pong: prepare next band while DMA sends this one
    }
    xSemaphoreTake(flush_done_sem, portMAX_DELAY);  // wait for final band
    lv_display_flush_ready(disp);
}
```

**Allocate at init:**
```c
flush_done_sem = xSemaphoreCreateBinary();
swap_buf[0] = heap_caps_malloc(LCD_H_RES * LINES_PER_BAND * 2, MALLOC_CAP_DMA);
swap_buf[1] = heap_caps_malloc(LCD_H_RES * LINES_PER_BAND * 2, MALLOC_CAP_DMA);
```

### Why This Works (and What Doesn't)

| Approach | Works? | Why |
|----------|--------|-----|
| Partial buffer (default) | **NO** | RASET bug → shredded display |
| `full_refresh` + `swap_bytes` | **NO** | Software swap on 300KB buffer triggers watchdog |
| `full_refresh` + `LV_COLOR_16_SWAP` | **NO** | LVGL v9 full_refresh re-renders entire screen every frame → watchdog |
| `direct_mode` (unmodified esp_lvgl_port) | **NO** | esp_lvgl_port only does full-screen flush for RGB/DSI, not SPI |
| `direct_mode` + `LV_COLOR_16_SWAP` | **NO** | LV_COLOR_16_SWAP swaps at render time; in direct_mode only dirty pixels re-render, so unchanged pixels are already swapped but newly rendered pixels get double-swapped → mixed byte order |
| **`direct_mode` + custom flush + bounce buffer swap** | **YES** | LVGL renders native byte order → flush copies bands to SRAM, swaps there, sends via DMA → SPIRAM framebuffer stays clean |

### Byte-Swap Details

`CONFIG_LV_COLOR_16_SWAP` was a Kconfig option in LVGL v8 (compile-time, zero cost). Removed from Kconfig in v9. The `#define` still exists in v9 code ([PR #6225](https://github.com/lvgl/lvgl/issues/6317)) but is **incompatible with `direct_mode`** because it modifies pixels at render time, corrupting the persistent framebuffer.

The working solution does the swap in the flush callback on a per-band copy:
- 40 rows × 320px = 12,800 pixels per band → swap takes ~25µs (negligible)
- The SPIRAM framebuffer is never modified by the swap
- DMA sends from the SRAM bounce buffer (DMA-capable, fast)

**Additional references:**
- [LVGL Issue #6317](https://github.com/lvgl/lvgl/issues/6317) — LV_COLOR_16_SWAP restoration discussion
- [LVGL Issue #6104](https://github.com/lvgl/lvgl/issues/6104) — CONFIG_LV_COLOR_16_SW_SWAP added
- [esp-bsp Issue #154](https://github.com/espressif/esp-bsp/issues/154) — SIMD acceleration for byte swapping

---

## Part A: Replace Custom Components with Registry Equivalents

### A1. `main/idf_component.yml` — Pin all dependencies to exact versions
```yaml
dependencies:
  lvgl/lvgl: "9.5.0"                             # was "^8"
  espressif/esp_lvgl_port: "2.7.2"               # NEW — replaces custom esp_lv_port
  espressif/esp_lcd_axs15231b: "2.1.0"           # was "*" (also provides touch driver)
  espressif/esp_io_expander_tca9554: "2.0.3"     # was "*"
```

### A2. Delete `components/esp_lv_port/` directory entirely
The registry's `espressif/esp_lvgl_port` replaces all of this.

### A3. Delete touch code from BSP component
- Remove `bsp_touch.c` and `bsp_touch.h`
- Update CMakeLists.txt to remove them from SRCS

The standard `esp_lcd_touch_new_i2c_axs15231b()` from the `esp_lcd_axs15231b` component replaces the custom raw I2C polling.

### A4. Update CMakeLists.txt REQUIRES
- `"esp_lv_port"` → `"esp_lvgl_port"`
- `"lvgl__lvgl"` → `"lvgl"` (component name format changed with registry v9 packages)

### A5. `sdkconfig.defaults` — Remove v8-only Kconfig options
- **Remove** `CONFIG_LV_MEM_CUSTOM=y` (v9 uses stdlib by default)
- **Remove** `CONFIG_LV_TXT_ENC_UTF8=y` (v9 always UTF-8)
- **Remove** `CONFIG_LV_COLOR_16_SWAP=y` (no longer a Kconfig option — use `add_compile_definitions` instead)
- **Keep:** font configs, `CONFIG_LV_USE_PERF_MONITOR`, `CONFIG_LV_USE_CANVAS`

### A6. Top-level `CMakeLists.txt`
No `LV_COLOR_16_SWAP` needed — byte swap is handled in the custom flush callback's bounce buffer (see display section above). `LV_COLOR_16_SWAP` is incompatible with `direct_mode`.

---

## Part B: Rewrite main.c (or equivalent init code)

### Includes
```c
// Remove:
#include "lv_port.h"
#include "bsp_touch.h"

// Add:
#include "esp_lvgl_port.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_axs15231b.h"
```

### Remove custom `touchpad_read()` callback entirely
The registry's `lvgl_port_add_touch()` handles the LVGL input callback internally.

### Type changes
- `lv_disp_t *` → `lv_display_t *`
- Remove `#define DISPLAY_ROTATION LV_DISP_ROT_NONE` (no longer needed)

### New touch initialization

**LESSON LEARNED:** The default `ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG()` macro does NOT set `scl_speed_hz`, which defaults to 0 and causes `i2c_master_bus_add_device()` to fail with "invalid scl frequency". You MUST set it manually after the macro:

```c
esp_lcd_panel_io_handle_t touch_io_handle;
esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
touch_io_config.scl_speed_hz = 400000;  // REQUIRED — macro leaves this at 0
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &touch_io_config, &touch_io_handle));

esp_lcd_touch_handle_t touch_handle;
esp_lcd_touch_config_t tp_cfg = {
    .x_max = LCD_H_RES,
    .y_max = LCD_V_RES,
    .rst_gpio_num = GPIO_NUM_NC,
    .int_gpio_num = GPIO_NUM_NC,    // polling mode
    .flags = {
        .swap_xy = 0,
        .mirror_x = 0,
        .mirror_y = 0,
    },
};
ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_axs15231b(touch_io_handle, &tp_cfg, &touch_handle));
```

**LESSON LEARNED:** The `_EX` variant macro (`ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG_EX(400000)`) has a bug — its parameter name `scl_speed_hz` collides with the struct field `.scl_speed_hz`, causing a compile error. Use the base macro + manual assignment instead.

### Display config (registry component)
```c
// See "AXS15231B QSPI Display" section above for the full working solution
// including custom flush callback, bounce buffer, and DMA sync.
lvgl_port_display_cfg_t disp_cfg = {
    .io_handle = io_handle,
    .panel_handle = panel_handle,
    .buffer_size = LCD_BUFFER_SIZE,          // LCD_H_RES * LCD_V_RES (full screen)
    .trans_size = 0,                         // custom flush handles chunking
    .hres = LCD_H_RES,
    .vres = LCD_V_RES,
    .color_format = LV_COLOR_FORMAT_RGB565,
    .flags = {
        .buff_spiram = true,
        .direct_mode = true,     // REQUIRED — see display section for details
        // Do NOT set swap_bytes or full_refresh
    },
};
lvgl_disp = lvgl_port_add_disp(&disp_cfg);

// Override flush + DMA callbacks (see display section for implementation)
lv_display_set_flush_cb(lvgl_disp, axs15231b_flush_cb);
esp_lcd_panel_io_register_event_callbacks(io_handle, &io_cbs, lvgl_disp);
```

### Touch config (registry component)
```c
lvgl_port_touch_cfg_t touch_cfg = {
    .disp = lvgl_disp,
    .handle = touch_handle,
};
lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
```

---

## Part C: LVGL v9 Widget API Updates

### Widget renames
| v8 | v9 |
|----|-----|
| `lv_btn_create()` | `lv_button_create()` |
| `lv_obj_del()` | `lv_obj_delete()` |
| `lv_obj_move_foreground(obj)` | `lv_obj_move_to_index(obj, -1)` |
| `(lv_coord_t)` | `(int32_t)` |
| `lv_indev_get_act()` | `lv_indev_active()` |

### Include change
- `#include "lv_port.h"` → `#include "esp_lvgl_port.h"` (inside `#ifndef DESKTOP_BUILD`)

### Unchanged APIs
Animation API, styling, flex layout, events, colors, `LV_OPA_50`, `LV_SYMBOL_REFRESH`, `lv_color_make()`, `lv_color_white()`, `lv_scr_act()` — all the same in v9.

---

## Part D: Desktop Build Updates

### `lv_conf.h` — Remove v8 options, rename widgets
**Remove:**
- `LV_COLOR_16_SWAP`
- `LV_MEM_CUSTOM` block (malloc/free/realloc)
- `LV_DPI_DEF`
- `LV_TXT_ENC`
- `LV_USE_COLORWHEEL`

**Rename:**
| v8 | v9 |
|----|-----|
| `LV_USE_BTN` | `LV_USE_BUTTON` |
| `LV_USE_BTNMATRIX` | `LV_USE_BUTTONMATRIX` |
| `LV_USE_IMG` | `LV_USE_IMAGE` |
| `LV_USE_IMGBTN` | `LV_USE_IMAGEBUTTON` |
| `LV_USE_ANIMIMG` | `LV_USE_ANIMIMAGE` |

### Desktop `main.c` — v9 display/input API

**Display init:**
```c
// v8: lv_disp_draw_buf_t, lv_disp_drv_t, lv_color_t buf[]
// v9:
uint8_t buf1[DISP_HOR_RES * DISP_VER_RES * 2];  // raw bytes, not lv_color_t

lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
lv_display_set_flush_cb(disp, sdl_flush_cb);
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
lv_display_set_buffers(disp, buf1, NULL,
                       DISP_HOR_RES * DISP_VER_RES * 2,
                       LV_DISPLAY_RENDER_MODE_FULL);
```

**Input init:**
```c
// v8: lv_indev_drv_t, lv_indev_drv_init(), lv_indev_drv_register()
// v9:
lv_indev_t *mouse_indev = lv_indev_create();
lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(mouse_indev, sdl_mouse_read_cb);
```

**Callback signatures:**
```c
// v8: void flush_cb(lv_disp_drv_t *drv, ..., lv_color_t *color_p)
// v9: void flush_cb(lv_display_t *disp, ..., uint8_t *px_map)
//     lv_disp_flush_ready(drv) → lv_display_flush_ready(disp)
//     pitch: w * sizeof(lv_color_t) → w * 2

// v8: void read_cb(lv_indev_drv_t *drv, ...)
// v9: void read_cb(lv_indev_t *indev, ...)
//     LV_INDEV_STATE_PR → LV_INDEV_STATE_PRESSED
//     LV_INDEV_STATE_REL → LV_INDEV_STATE_RELEASED
```

---

## Part E: Rename BSP Files (optional — drop misleading `bsp_` prefix)

The `bsp_` prefix implies membership in Espressif's BSP framework, but these files are just board-specific hardware init. Rename to clarify ownership:

| Old | New |
|-----|-----|
| `bsp_i2c.c` / `.h` | `hw_i2c.c` / `.h` |
| `bsp_display.c` / `.h` | `hw_display.c` / `.h` |
| `bsp_axp2101.cpp` / `.h` | `hw_pmic.cpp` / `.h` |
| `components/esp_bsp/` | `components/board_hw/` |

Function renames:
| Old | New |
|-----|-----|
| `bsp_i2c_init()` | `hw_i2c_init()` |
| `bsp_i2c_lock()` / `bsp_i2c_unlock()` | `hw_i2c_lock()` / `hw_i2c_unlock()` |
| `bsp_display_init()` | `hw_display_init()` |
| `bsp_display_brightness_init()` | `hw_display_brightness_init()` |
| `bsp_display_set_brightness()` | `hw_display_set_brightness()` |
| `bsp_display_get_brightness()` | `hw_display_get_brightness()` |
| `bsp_axp2101_init()` | `hw_pmic_init()` |

---

## Part F: Docker Build — Avoid Root-Owned Files

**Problem:** Docker containers default to root. Build artifacts in mounted workspace end up owned by `root:root` on the host, requiring `sudo rm` to clean.

**Solution applied in 2048-esp32:**

1. Pass `--user $(id -u):$(id -g)` to all `docker run` commands
2. Set `HOME=/tmp/build-home` (writable by any UID, no `/etc/passwd` entry needed)
3. Use a **host bind mount** for the build cache instead of a Docker named volume (named volumes initialize as root:root)
4. Use `bash -c` instead of `bash -lc` (no login shell — avoids `.bash_profile` errors)
5. In entry scripts:
   - `mkdir -p "$HOME"` to create writable home
   - `export GIT_CONFIG_GLOBAL="$HOME/.gitconfig"` for git safe.directory config
   - `export XDG_CACHE_HOME="/cache"` to point ESP component manager at the cache mount
   - `export CCACHE_DIR="/cache/ccache"` for ccache persistence

**LESSON LEARNED:** Docker named volumes (`-v volname:/path`) are always initialized with root ownership. Even mounting at `/tmp/...` doesn't help because Docker creates the mount point directory as root. Use host bind mounts (`-v /host/path:/container/path`) instead — the host directory inherits the creating user's ownership.

---

## Verification Checklist
1. **ESP32 build**: Delete `build/`, `sdkconfig`, `managed_components/`, run full build
2. **Desktop build**: Rebuild with cmake (requires system LVGL v9)
3. **Runtime on device**:
   - Display colors correct (compile-time `LV_COLOR_16_SWAP`)
   - No watchdog timeouts (check serial log)
   - Touch works (coordinates map correctly in portrait mode)
   - Gestures in all 4 directions
   - Tile slide animations
   - Overlay transparency (win/lose screens)
   - New Game button
4. **File ownership**: `ls -la build/` shows host user, not root

## Files Summary

| Action | File |
|--------|------|
| **Delete** | `components/esp_lv_port/` (entire directory) |
| **Delete** | `bsp_touch.c`, `bsp_touch.h` |
| **Rename** | `components/esp_bsp/` → `components/board_hw/` (all files `bsp_*` → `hw_*`) |
| **Edit** | Top-level `CMakeLists.txt` (add `LV_COLOR_16_SWAP=1`) |
| **Edit** | `idf_component.yml` |
| **Edit** | `main.c` (major rewrite of setup + include/function renames) |
| **Edit** | Board HW `CMakeLists.txt` (new filenames + component name) |
| **Edit** | Game component `CMakeLists.txt` |
| **Edit** | Game UI source (widget API renames) |
| **Edit** | Game gesture source (`lv_indev_get_act` → `lv_indev_active`) |
| **Edit** | `sdkconfig.defaults` |
| **Edit** | Desktop `lv_conf.h` |
| **Edit** | Desktop `main.c` |
| **Edit** | `.gitignore` (add `.ccache/`) |
| **Edit** | Makefile / Docker scripts (non-root build) |
