cmake_minimum_required(VERSION 3.26)

project(js C CXX)

include(cmake/gn/gn.cmake)

if(NOT TARGET c++)
  add_gn_target(c++ prebuilds:c++)
endif()

if(NOT TARGET v8)
  add_gn_target(v8 prebuilds:v8)

  target_link_libraries(
    v8
    INTERFACE
      c++
  )
endif()

if(NOT TARGET uv)
  add_subdirectory(vendor/libuv EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET utf)
  add_subdirectory(vendor/libutf EXCLUDE_FROM_ALL)
endif()

add_library(js OBJECT)

set_target_properties(
  js
  PROPERTIES
  C_STANDARD 11
  CXX_STANDARD 20
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  js
  INTERFACE
    include/js.h
    include/js/ffi.h
  PRIVATE
    src/js.cc
)

target_include_directories(
  js
  PUBLIC
    include
    $<TARGET_PROPERTY:c++,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:utf,INTERFACE_INCLUDE_DIRECTORIES>
  PRIVATE
    $<TARGET_PROPERTY:v8,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(
  js
  PUBLIC
    $<TARGET_PROPERTY:c++,INTERFACE_COMPILE_DEFINITIONS>
  PRIVATE
    $<TARGET_PROPERTY:v8,INTERFACE_COMPILE_DEFINITIONS>
)

if(MSVC)
  target_compile_options(
    js
    PUBLIC
      /GR-
      /X
      /MT$<$<CONFIG:Debug>:d>
  )

  target_link_options(
    js
    PUBLIC
      /DEFAULTLIB:libcpmt$<$<CONFIG:Debug>:d>
  )
else()
  target_compile_options(
    js
    PUBLIC
      -fno-rtti
      -nostdinc++
  )
endif()

add_library(js_shared SHARED)

set_target_properties(
  js_shared
  PROPERTIES
  OUTPUT_NAME js
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  js_shared
  PUBLIC
    js
    uv
    v8
    utf_shared
)

add_library(js_static STATIC)

set_target_properties(
  js_static
  PROPERTIES
  OUTPUT_NAME js
  PREFIX lib
)

target_link_libraries(
  js_static
  PUBLIC
    js
    uv_a
    v8
    utf_static
)

install(TARGETS js_shared js_static)

install(FILES include/js.h DESTINATION include)

install(DIRECTORY include/js DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
