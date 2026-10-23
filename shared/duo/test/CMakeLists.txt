CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
project(duo-tests)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

add_library(unity_framework
  ${CMAKE_CURRENT_LIST_DIR}/unity/unity.c
)

target_include_directories(unity_framework PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/unity
)

add_library(firmware
  ${CMAKE_CURRENT_LIST_DIR}/../seq.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../drum_kit.cpp
)

target_include_directories(firmware PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../
)

add_executable(sequencer_test 
  ${CMAKE_CURRENT_LIST_DIR}/firmware/sequencer_test.cpp
)
target_link_libraries(sequencer_test PRIVATE unity_framework firmware)

add_test(
  NAME sequencer_test
  COMMAND $<TARGET_FILE:sequencer_test>)

add_executable(drum_pads_test
  ${CMAKE_CURRENT_LIST_DIR}/firmware/drum_pads_test.cpp
)
target_link_libraries(drum_pads_test PRIVATE unity_framework firmware)
add_test(NAME drum_pads_test COMMAND $<TARGET_FILE:drum_pads_test>)

add_executable(drum_kit_test
  ${CMAKE_CURRENT_LIST_DIR}/firmware/drum_kit_test.cpp
)
target_link_libraries(drum_kit_test PRIVATE unity_framework firmware)
add_test(NAME drum_kit_test COMMAND $<TARGET_FILE:drum_kit_test>)

# Not a test: writes each drum to a .wav file to listen to on a computer.
# Run: build/render_drums <output folder>
add_executable(render_drums
  ${CMAKE_CURRENT_LIST_DIR}/firmware/render_drums.cpp
)
target_link_libraries(render_drums PRIVATE firmware)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -Wall -Wpedantic -Werror -std=gnu99" CACHE INTERNAL "c compiler flags release")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -Wall -Wpedantic -Werror -std=c++11 -Wpedantic" CACHE INTERNAL "cxx compiler flags release")

add_definitions(
  -DUNITY_INCLUDE_CONFIG_H
)
