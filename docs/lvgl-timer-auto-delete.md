# LVGL Timer auto_delete Pitfall

## Summary

`lv_timer_create()` defaults to `auto_delete = true`. When a timer's `repeat_count` reaches 0, LVGL **deletes the timer object and frees its memory**. Any subsequent access to the timer handle is a use-after-free that silently corrupts the heap.

## The Bug

```c
/* Create a one-shot timer */
idle_timer = lv_timer_create(my_callback, 30000, NULL);
lv_timer_set_repeat_count(idle_timer, 1);

/* ... 30 seconds later, the timer fires and LVGL deletes it ... */

/* Later, try to reuse the timer — USE-AFTER-FREE */
lv_timer_reset(idle_timer);              /* writes to freed memory */
lv_timer_set_repeat_count(idle_timer, 1); /* writes to freed memory */
lv_timer_resume(idle_timer);             /* writes to freed memory */
```

The heap corruption doesn't crash immediately. It corrupts TLSF free-list metadata, and the crash surfaces later during an unrelated `malloc()` call — often deep in LVGL's rendering pipeline (`circ_calc_aa4`, `lv_draw_sw_fill`, etc.), making the root cause extremely difficult to trace.

## Symptoms

- `Guru Meditation Error: StoreProhibited` or `LoadProhibited` during LVGL rendering
- `CORRUPT HEAP: Invalid data at 0x... Expected 0xfefefefe` (with heap poisoning enabled)
- Crash in `remove_free_block` in TLSF allocator (either LVGL's built-in or ESP-IDF's system heap)
- Crash address (`EXCVADDR`) is a small value like `0x0000XXXX` — a corrupted pointer being dereferenced
- The crash occurs at a seemingly random time after the actual corruption, often triggered by a burst of allocations (e.g. screen redraw)

## The Fix

If you need to reuse a timer handle after it fires, disable auto-deletion:

```c
idle_timer = lv_timer_create(my_callback, 30000, NULL);
lv_timer_set_auto_delete(idle_timer, false);  /* CRITICAL */
lv_timer_set_repeat_count(idle_timer, 1);
```

With `auto_delete = false`, the timer pauses when `repeat_count` reaches 0 instead of being deleted. You can then safely reset and resume it.

## Debugging Approach

This bug was found through a systematic process:

1. **Observed**: screensaver worked, but returning to game caused a crash/reboot
2. **Tried**: different overlay approaches (child of screen, `lv_layer_top()`, `lv_screen_load()`), different buffer sizes, different LVGL allocators — all crashed identically
3. **Enabled** `CONFIG_HEAP_POISONING_COMPREHENSIVE=y` in sdkconfig — confirmed heap corruption with exact address
4. **Reduced** screensaver to absolute minimum (just change background color, no animation) — still crashed
5. **Realized** the corruption happened regardless of what the screensaver rendered, pointing to timer/lifecycle code rather than rendering
6. **Found** LVGL's `auto_delete = true` default in `lv_timer.c:179` and the deletion at `lv_timer.c:369-372`

Key lesson: when heap corruption symptoms point to rendering code, the actual bug may be in unrelated lifecycle/timer code. The corruption site and the crash site can be far apart in both code and time.

## LVGL Version

Confirmed in LVGL v9.5.0 (`lvgl/lvgl` component registry). The `auto_delete` default has been `true` since at least v9.0.

## Applicability

This applies to any LVGL timer that is:
- Created with `lv_timer_create()`
- Given a finite `repeat_count` via `lv_timer_set_repeat_count()`
- Referenced by a stored pointer after the timer has fired

The same pattern would affect the seedsigner-c-modules project if LVGL timers with repeat counts are used for screensavers, timeouts, or one-shot events.
