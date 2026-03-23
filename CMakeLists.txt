cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BOARD)
    set(BOARD "waveshare_s3_lcd35b")
endif()
message(STATUS "Building for board: ${BOARD}")

# Disable peripherals not used by this game
add_compile_definitions(
    BOARD_HAS_CAMERA=0
    BOARD_HAS_SDCARD=0
    BOARD_HAS_AUDIO=0
    BOARD_HAS_RTC=0
    BOARD_HAS_IMU=0
)

set(EXTRA_COMPONENT_DIRS "components")
set(SDKCONFIG_DEFAULTS
    "sdkconfig.defaults"
    "components/esp-board-common/boards/${BOARD}/sdkconfig.defaults"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(game_2048)
