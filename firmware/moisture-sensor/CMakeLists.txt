cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
)

set(COMPONENTS src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(moisture_sensor)