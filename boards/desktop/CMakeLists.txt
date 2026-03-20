cmake_minimum_required(VERSION 3.16)
project(game_2048_desktop C)

set(CMAKE_C_STANDARD 11)

# Repo root (two levels up from boards/desktop/)
set(REPO_ROOT ${CMAKE_SOURCE_DIR}/../..)

# ── Find dependencies ──
if(WIN32)
    find_package(SDL2 REQUIRED CONFIG)

    find_path(SDL2_PARENT_INCLUDE_DIR SDL2/SDL.h
        HINTS ${CMAKE_PREFIX_PATH}
        PATH_SUFFIXES include)
    if(NOT SDL2_PARENT_INCLUDE_DIR)
        message(FATAL_ERROR "Could not find SDL2/SDL.h — set CMAKE_PREFIX_PATH to SDL2 install")
    endif()

    find_path(LVGL_INCLUDE_DIR lvgl/lvgl.h HINTS ${CMAKE_PREFIX_PATH}/include)
    find_library(LVGL_LIBRARY NAMES lvgl HINTS ${CMAKE_PREFIX_PATH}/lib)
    if(NOT LVGL_INCLUDE_DIR OR NOT LVGL_LIBRARY)
        message(FATAL_ERROR "LVGL not found. Set CMAKE_PREFIX_PATH to the LVGL install directory.")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(LVGL REQUIRED lvgl)
endif()

# ── Source files ──
set(GAME_DIR ${REPO_ROOT}/components/game_2048)
set(FONT_DIR ${REPO_ROOT}/fonts)

set(GAME_SOURCES
    ${GAME_DIR}/game_logic.c
    ${GAME_DIR}/game_ui.c
    ${GAME_DIR}/game_gesture.c
    ${GAME_DIR}/game_screensaver.c
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
    board_init.c
    ${REPO_ROOT}/main/game_main.c
    ${GAME_SOURCES}
)

target_compile_definitions(game_2048_desktop PRIVATE
    DESKTOP_BUILD
    LV_CONF_INCLUDE_SIMPLE
)

if(WIN32)
    target_include_directories(game_2048_desktop PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${REPO_ROOT}/components/board_common
        ${GAME_DIR}/include
        ${SDL2_PARENT_INCLUDE_DIR}
        ${LVGL_INCLUDE_DIR}
        ${LVGL_INCLUDE_DIR}/lvgl
    )
    target_link_libraries(game_2048_desktop PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        ${LVGL_LIBRARY}
    )
else()
    target_include_directories(game_2048_desktop PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${REPO_ROOT}/components/board_common
        ${GAME_DIR}/include
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
endif()
