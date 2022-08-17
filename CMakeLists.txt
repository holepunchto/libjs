cmake_minimum_required(VERSION 3.22)

project(js C CXX)

find_library(V8 v8 REQUIRED)
find_path(V8_INCLUDE_DIR v8.h)

add_library(js OBJECT)

set_target_properties(
  js
  PROPERTIES
  C_STANDARD 99
  CXX_STANDARD 14
  POSITION_INDEPENDENT_CODE 1
)

target_sources(
  js
  INTERFACE
    include/js.h
  PRIVATE
    src/env.cc
    src/handle.cc
    src/js.cc
    src/run.cc
    src/value.cc
    src/platform/platform.cc
    src/platform/task-runner.cc
    src/platform/tracing-controller.cc
)

target_include_directories(
  js
  PUBLIC
    include
    ${V8_INCLUDE_DIR}
)

target_compile_definitions(
  js
  PUBLIC
    V8_COMPRESS_POINTERS=1
)

add_library(js_shared SHARED $<TARGET_OBJECTS:js>)

set_target_properties(
  js_shared
  PROPERTIES
  OUTPUT_NAME js
)

target_link_libraries(
  js_shared
  PUBLIC
    ${V8}
)

add_library(js_static STATIC $<TARGET_OBJECTS:js>)

set_target_properties(
  js_static
  PROPERTIES
  OUTPUT_NAME js
)

target_link_libraries(
  js_static
  PUBLIC
    ${V8}
)

install(TARGETS js_shared js_static)

install(FILES include/js.h DESTINATION include)

enable_testing()

add_subdirectory(test)
