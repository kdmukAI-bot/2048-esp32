cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BOARD)
    set(BOARD "waveshare_s3_lcd35b")
endif()
message(STATUS "Building for board: ${BOARD}")

set(EXTRA_COMPONENT_DIRS "components")
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;boards/${BOARD}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(game_2048)
