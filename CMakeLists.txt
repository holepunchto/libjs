cmake_minimum_required(VERSION 3.25)

project(js C CXX)

include(cmake/gn/gn.cmake)

if(MSVC)
  add_compile_options(/MT$<$<CONFIG:Debug>:d>)

  add_link_options(/DEFAULTLIB:libcpmt$<$<CONFIG:Debug>:d>.lib)
endif()

if(NOT TARGET c++)
  add_gn_library(c++ prebuilds:c++ STATIC)
endif()

if(NOT TARGET v8)
  add_gn_library(v8 prebuilds:v8 STATIC)

  target_link_libraries(
    v8
    INTERFACE
      c++
  )
endif()

if(NOT TARGET mem)
  add_subdirectory(vendor/libmem EXCLUDE_FROM_ALL)
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
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:utf,INTERFACE_INCLUDE_DIRECTORIES>
  PRIVATE
    $<TARGET_PROPERTY:mem,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
  js
  PRIVATE
    v8
)

if(MSVC)
  target_compile_options(
    js
    PUBLIC
      /GR-
      /X
  )
else()
  target_compile_options(
    js
    PUBLIC
      -fno-rtti
      -nostdinc++
  )
endif()

add_library(js_shared SHARED $<TARGET_OBJECTS:js>)

set_target_properties(
  js_shared
  PROPERTIES
  OUTPUT_NAME js
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  js_shared
  PUBLIC
    uv
    v8
    mem_shared
    utf_shared
    ${link_libraries}
)

add_library(js_static STATIC $<TARGET_OBJECTS:js>)

set_target_properties(
  js_static
  PROPERTIES
  OUTPUT_NAME js
  PREFIX lib
)

target_link_libraries(
  js_static
  PUBLIC
    uv_a
    v8
    mem_static
    utf_static
    ${link_libraries}
)

install(TARGETS js_shared js_static)

install(FILES include/js.h DESTINATION include)

install(DIRECTORY include/js DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
