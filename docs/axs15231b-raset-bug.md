# AXS15231B QSPI RASET Bug

## Summary

The AXS15231B display controller **ignores the RASET (Row Address Set, 0x2B) command when using a QSPI interface**. This is a hardware-level limitation — the command is accepted but has no effect. Column address (CASET, 0x2A) works normally.

This means you cannot define an arbitrary row window for partial screen updates over QSPI. The controller always expects pixel data covering the full column height starting from row 0.

## Evidence in the Espressif Driver

The official `esp_lcd_axs15231b` driver (v2.1.0) explicitly skips RASET in QSPI mode:

```c
// managed_components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c, line 328
if (0 == axs15231b->flags.use_qspi_interface) {
    tx_param(axs15231b, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);
}
```

RASET is only sent for SPI (non-QSPI) interfaces. For QSPI, it is intentionally omitted.

## The RAMWR/RAMWRC Workaround

The driver uses a sequential write strategy to compensate:

```c
// line 339
if (y_start == 0) {
    tx_color(axs15231b, io, LCD_CMD_RAMWR, color_data, len);   // 0x2C: start new frame write
} else {
    tx_color(axs15231b, io, LCD_CMD_RAMWRC, color_data, len);  // 0x3C: continue frame write
}
```

- **RAMWR (0x2C)**: Resets the write pointer to (column_start, row 0) and begins writing
- **RAMWRC (0x3C)**: Continues writing from wherever the previous write left off

This enables **sequential top-down band rendering**: the first band starting at y=0 uses RAMWR, and subsequent bands use RAMWRC to continue filling the framebuffer in order. The bands must be sent top-to-bottom without gaps.

## Impact on LVGL Rendering Strategies

### full_refresh = 1 (safe, slow)
Always sends the entire 320×480 framebuffer every frame. Guaranteed correct but CPU-intensive — 153,600 pixels × 2 bytes = 307KB per frame must be copied from PSRAM to DMA buffers and transferred over QSPI.

### full_refresh = 0, partial refresh (risky)
LVGL v8's default rendering divides dirty areas into horizontal bands and flushes them top-to-bottom, which *can* align with the RAMWR/RAMWRC mechanism. However, there is no guarantee that LVGL will always produce contiguous top-down bands covering the full row range — dirty areas may be scattered (e.g., score label at top + tile at bottom), leading to gaps that corrupt the display.

### direct_mode = 1 (potential best path)
LVGL's direct mode gives the flush callback access to the full framebuffer and dirty area list. The flush callback can then send the entire screen (like full_refresh) but with more control over the transfer strategy. Community reports suggest 22-30 FPS vs 4-8 FPS for standard full refresh on similar AXS15231B hardware.

## What Does NOT Work Over QSPI

- **Arbitrary rectangle updates**: Sending a small rectangle at (50, 200) to (150, 300) will not draw at that position — data goes to wherever the write pointer happens to be
- **RASET command**: Silently ignored
- **swap_xy / mirror**: The driver's `panel_axs15231b_swap_xy` and `panel_axs15231b_mirror` functions have no effect in QSPI mode (confirmed in esp-iot-solution issue #579)
- **Software rotation via per-pixel transforms in flush callback**: While technically possible, the CPU cost of transforming 153,600 pixels per frame on the ESP32-S3 causes watchdog timeouts

## References

- **Espressif driver source**: `managed_components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c` (lines 309-346)
- **esp-iot-solution Issue #579**: draw_bitmap with arbitrary coordinates has no effect in QSPI mode
  https://github.com/espressif/esp-iot-solution/issues/579
- **lvgl_micropython Discussion #161**: Documents RASET being commented out, workaround discussion
  https://github.com/lvgl-micropython/lvgl_micropython/discussions/161
- **lvgl_micropython Issue #381**: Rotation not possible due to QSPI limitations
  https://github.com/lvgl-micropython/lvgl_micropython/issues/381
- **LVGL Forum — JC3248W535 AXS15231B driver**: Performance comparison (direct mode 22-30 FPS vs full refresh 4-8 FPS), confirms CASET/RASET ineffective
  https://forum.lvgl.io/t/jc3248w535-axs15231b-driver/18707
- **AXS15231B Datasheet V0.5**:
  https://dl.espressif.com/AE/esp_iot_solution/AXS15231B_Datasheet_V0.5_20230306.pdf
- **ESP Component Registry — esp_lcd_axs15231b**:
  https://components.espressif.com/components/espressif/esp_lcd_axs15231b
