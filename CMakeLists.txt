cmake_minimum_required(VERSION 3.25)

project(js C CXX)

if(NOT TARGET v8)
  find_program(v8_prebuilds NAMES v8-prebuilds)

  execute_process(
    COMMAND ${v8_prebuilds} paths lib
    OUTPUT_VARIABLE v8
  )

  execute_process(
    COMMAND ${v8_prebuilds} paths include
    OUTPUT_VARIABLE v8_include
  )

  add_library(v8 STATIC IMPORTED)

  set_target_properties(
    v8
    PROPERTIES
    IMPORTED_LOCATION ${v8}
  )

  target_include_directories(
    v8
    INTERFACE
      ${v8_include}
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
  C_STANDARD 99
  CXX_STANDARD 17
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
    $<TARGET_PROPERTY:v8,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:mem,INTERFACE_INCLUDE_DIRECTORIES>
)

if(MSVC)
  target_compile_options(
    js
    PUBLIC
      /GR-
  )
else()
  target_compile_options(
    js
    PUBLIC
      -fno-rtti
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
)

if(WIN32)
  target_link_libraries(
    js_shared
    PUBLIC
      dbghelp.lib
      winmm.lib
      ws2_32.lib
  )
endif()

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
)

if(WIN32)
  target_link_libraries(
    js_static
    PUBLIC
      dbghelp.lib
      winmm.lib
      ws2_32.lib
  )
endif()

install(TARGETS js_shared js_static)

install(FILES include/js.h DESTINATION include)

install(DIRECTORY include/js DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
