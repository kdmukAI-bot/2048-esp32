cmake_minimum_required(VERSION 3.16)
project(game_2048_desktop C)

set(CMAKE_C_STANDARD 11)

# ── Find dependencies ──
if(WIN32)
    # Windows: SDL2 ships cmake config; LVGL is found manually
    find_package(SDL2 REQUIRED CONFIG)

    # SDL2::SDL2 sets include dir to .../include/ but our code uses
    # #include <SDL2/SDL.h>, so we need the parent directory
    get_target_property(_sdl2_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if(_sdl2_inc)
        foreach(_dir ${_sdl2_inc})
            get_filename_component(_parent "${_dir}" DIRECTORY)
            list(APPEND SDL2_PARENT_DIRS "${_parent}")
        endforeach()
    endif()

    # LVGL installed to CMAKE_PREFIX_PATH — find headers and static lib
    find_path(LVGL_INCLUDE_DIR lvgl/lvgl.h HINTS ${CMAKE_PREFIX_PATH}/include)
    find_library(LVGL_LIBRARY NAMES lvgl HINTS ${CMAKE_PREFIX_PATH}/lib)
    if(NOT LVGL_INCLUDE_DIR OR NOT LVGL_LIBRARY)
        message(FATAL_ERROR "LVGL not found. Set CMAKE_PREFIX_PATH to the LVGL install directory.")
    endif()
else()
    # Linux/macOS: use pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(LVGL REQUIRED lvgl)
endif()

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

if(WIN32)
    target_include_directories(game_2048_desktop PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${GAME_DIR}/include
        ${SDL2_PARENT_DIRS}
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
