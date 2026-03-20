# CST816D Touch Controller Report Rate Limitation

## Summary

The CST816D capacitive touch controller (used on the Waveshare ESP32-S3 Touch LCD 2) has a low scan/report rate (~25-60Hz). Fast swipes complete before the controller captures enough samples, so the touch event is never reported to the firmware. This is a hardware limitation — no software workaround can recover touches the controller doesn't report.

## Observed Behavior

When swiping at a speed that works reliably on the AXS15231B touch controller (Waveshare ESP32-S3 Touch LCD 3.5B), the CST816D frequently fails to report the touch at all. No I2C touch data is generated for these fast swipes.

Debug logging confirmed that every touch event the CST816D *does* report is correctly processed and results in a game move. The missing swipes simply never appear as press/release events in the firmware.

## Mitigations

### Custom gesture detection (implemented)

LVGL's built-in `LV_EVENT_GESTURE` requires seeing pointer movement across multiple polling cycles while pressed. Even when the CST816D reports a fast swipe, LVGL often had only 1-2 coordinate samples — not enough to enter gesture mode.

The custom gesture detector in `game_gesture.c` bypasses LVGL's gesture system entirely. It tracks raw press/release coordinates and fires a swipe when the total displacement exceeds 15px. This catches every swipe the controller reports, regardless of how few intermediate samples exist.

This also improves swipe responsiveness on the 3.5B board (AXS15231B touch), where occasional fast swipes were similarly missed by LVGL's gesture system despite the controller's higher report rate.

### User adaptation

Users need to swipe slightly slower on the 2" board compared to the 3.5B. A deliberate swipe across ~1/3 of the screen is detected reliably; a quick flick may not be.

## Applicability

This limitation applies to any board using CST816S/CST816D family touch controllers. The same issue would affect CST816D boards in the seedsigner-c-modules project if touch gestures are used.
