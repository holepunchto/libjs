cmake_minimum_required(VERSION 3.22)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(js C CXX)

if(NOT DEFINED V8)
  execute_process(
    COMMAND v8-prebuilds-paths --lib
    OUTPUT_VARIABLE V8
  )

  execute_process(
    COMMAND v8-prebuilds-paths --include
    OUTPUT_VARIABLE V8_INCLUDE
  )
endif()

add_library(v8 STATIC IMPORTED)

set_target_properties(
  v8
  PROPERTIES
  IMPORTED_LOCATION ${V8}
)

target_include_directories(
  v8
  INTERFACE
    ${V8_INCLUDE}
)

if(NOT TARGET mem)
  add_subdirectory(vendor/libmem EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET uv)
  add_subdirectory(vendor/libuv EXCLUDE_FROM_ALL)
endif()

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
    include/js/ffi.h
  PRIVATE
    src/js.cc
)

target_include_directories(
  js
  PUBLIC
    include
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
  PRIVATE
    $<TARGET_PROPERTY:v8,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:mem,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_options(
  js
  PUBLIC
    -fno-rtti
)

target_compile_definitions(
  js
  PUBLIC
    V8_COMPRESS_POINTERS
    V8_ENABLE_SANDBOX
)

add_library(js_shared SHARED $<TARGET_OBJECTS:js> $<TARGET_OBJECTS:mem>)

set_target_properties(
  js_shared
  PROPERTIES
  OUTPUT_NAME js
)

target_compile_definitions(
  js_shared
  PUBLIC
    $<TARGET_PROPERTY:js,INTERFACE_COMPILE_DEFINITIONS>
)

target_link_libraries(
  js_shared
  PUBLIC
    uv
    v8
)

add_library(js_static STATIC $<TARGET_OBJECTS:js> $<TARGET_OBJECTS:mem>)

set_target_properties(
  js_static
  PROPERTIES
  OUTPUT_NAME js
)

target_compile_definitions(
  js_static
  PUBLIC
    $<TARGET_PROPERTY:js,INTERFACE_COMPILE_DEFINITIONS>
)

target_link_libraries(
  js_static
  PUBLIC
    uv_a
    v8
)

install(TARGETS js_shared js_static)

install(FILES include/js.h DESTINATION include)

install(DIRECTORY include/js DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
