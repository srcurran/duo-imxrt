CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(duo)

ENABLE_LANGUAGE(ASM)

set(EXECUTABLE_NAME duo-imxrt.elf)

add_executable(${EXECUTABLE_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tempo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/adapters/fast_led/FastLED.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../../shared/duo/effect_custom_envelope.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../../shared/duo/seq.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../../
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}
    ${CMAKE_CURRENT_LIST_DIR}/../libraries/
    ${CMAKE_CURRENT_LIST_DIR}/../../core/adapters/fast_led
)

# Tone test build (brains2/apps/duo/tone_test.h): set DUO_TONE_TEST=1 in the
# environment when building. An environment variable rather than a CMake
# option, so it can't linger in the CMake cache and leak into normal builds.
if("$ENV{DUO_TONE_TEST}" STREQUAL "1")
  message(STATUS "*** TONE TEST BUILD ***")
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE TONE_TEST=1)
endif()

set(WITH_USB true)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/build_app.cmake)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}
)

