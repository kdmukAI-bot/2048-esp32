cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BOARD)
    set(BOARD "waveshare_s3_lcd35b")
endif()
message(STATUS "Building for board: ${BOARD}")

# Disable peripherals not used by this game
add_compile_definitions(
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

# ESP32-P4 chip revision. P4 builds target revision v3.x by default (see
# sdkconfig.defaults.esp32p4, which ESP-IDF applies on top of sdkconfig.defaults for
# an esp32p4 target). v3.x and the older v0.x/v1.x silicon cannot run the same image,
# so BOARD_VARIANT=PRE_REV3 appends sdkconfig.p4_pre_rev3 to build for the older chips.
# The fragment is appended last so it overrides the v3 default. The revision only
# takes effect on a freshly generated sdkconfig (idf.py set-target or fullclean), which
# scripts/build_firmware.sh always does.
if(DEFINED BOARD_VARIANT AND NOT BOARD_VARIANT STREQUAL "")
    if(BOARD_VARIANT STREQUAL "PRE_REV3")
        if(NOT BOARD MATCHES "^waveshare_p4_")
            message(FATAL_ERROR "BOARD_VARIANT=PRE_REV3 only applies to ESP32-P4 boards, "
                                "not ${BOARD}")
        endif()
        list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.p4_pre_rev3")
        message(STATUS "Board variant: PRE_REV3 (ESP32-P4 chip revisions v0.x/v1.x)")
    else()
        message(FATAL_ERROR "Unknown BOARD_VARIANT '${BOARD_VARIANT}' (supported: PRE_REV3)")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(game_2048)
