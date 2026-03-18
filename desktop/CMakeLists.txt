cmake_minimum_required(VERSION 3.16)
project(game_2048_desktop C)

set(CMAKE_C_STANDARD 11)

# ── Find dependencies ──
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(LVGL REQUIRED lvgl)

# ── Game source files (shared with ESP32 build) ──
set(GAME_DIR ${CMAKE_SOURCE_DIR}/../components/game_2048)
set(FONT_DIR ${CMAKE_SOURCE_DIR}/../fonts)

set(GAME_SOURCES
    ${GAME_DIR}/game_logic.c
    ${GAME_DIR}/game_ui.c
    ${GAME_DIR}/game_gesture.c
    ${FONT_DIR}/font_clear_sans_bold_20.c
    ${FONT_DIR}/font_clear_sans_bold_24.c
    ${FONT_DIR}/font_clear_sans_bold_28.c
    ${FONT_DIR}/font_clear_sans_bold_30.c
    ${FONT_DIR}/font_clear_sans_bold_40.c
    ${FONT_DIR}/font_clear_sans_bold_50.c
    ${FONT_DIR}/font_clear_sans_regular_14.c
)

# ── Desktop simulator executable ──
add_executable(game_2048_desktop
    main.c
    ${GAME_SOURCES}
)

target_compile_definitions(game_2048_desktop PRIVATE
    DESKTOP_BUILD
    LV_CONF_PATH=${CMAKE_SOURCE_DIR}/lv_conf.h
)

target_include_directories(game_2048_desktop PRIVATE
    ${CMAKE_SOURCE_DIR}            # lv_conf.h
    ${GAME_DIR}/include            # game_logic.h, game_ui.h, game_gesture.h
    ${SDL2_INCLUDE_DIRS}
    ${LVGL_INCLUDE_DIRS}
)

target_link_directories(game_2048_desktop PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${LVGL_LIBRARY_DIRS}
)

target_link_libraries(game_2048_desktop PRIVATE
    ${SDL2_LIBRARIES}
    ${LVGL_LIBRARIES}
)

target_compile_options(game_2048_desktop PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${LVGL_CFLAGS_OTHER}
)
