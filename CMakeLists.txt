cmake_minimum_required(VERSION 3.22)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(js C CXX)

find_library(V8 v8 REQUIRED)
find_path(V8_INCLUDE_DIR v8.h)

add_library(js OBJECT)

set_target_properties(
  js
  PROPERTIES
  C_STANDARD 99
  CXX_STANDARD 17
  POSITION_INDEPENDENT_CODE 1
)

target_sources(
  js
  INTERFACE
    include/js.h
  PRIVATE
    src/js.cc
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

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
