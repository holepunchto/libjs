cmake_minimum_required(VERSION 4.0)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-harden REQUIRED PATHS node_modules/cmake-harden)
find_package(cmake-prelude REQUIRED PATHS node_modules/cmake-prelude)
find_package(cmake-test REQUIRED PATHS node_modules/cmake-test)
find_package(cmake-gn REQUIRED PATHS node_modules/cmake-gn)

project(js LANGUAGES C CXX VERSION 0.0.0)

if(TARGET js)
  return()
endif()

fetch_package("github:libuv/libuv@1.53.0")

fetch_package("github:holepunchto/libutf")

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

add_library_targets(
  js
  HEADERS
    include/js.h
  FEATURES
    c_std_11
  SOURCES
    src/js.cc
  LINK
    utf::<TYPE>
  STATIC_LINK
    uv_a
    v8
  SHARED_LINK
    uv
    v8
  EXPORT OFF
)

harden(js)

target_compile_features(
  js
  PRIVATE
    cxx_std_20
)

target_include_directories(
  js
  PRIVATE
    $<TARGET_PROPERTY:v8,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(
  js
  PRIVATE
    $<TARGET_PROPERTY:v8,INTERFACE_COMPILE_DEFINITIONS>
)

target_include_directories(
  js_headers
  INTERFACE
    $<TARGET_PROPERTY:c++,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(
  js_headers
  INTERFACE
    $<TARGET_PROPERTY:c++,INTERFACE_COMPILE_DEFINITIONS>
)

target_compile_options(
  js_headers
  INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
)

if(WIN32)
  target_compile_options(
    js_headers
    INTERFACE
      /MT$<$<CONFIG:Debug>:d>
  )

  target_link_options(
    js_headers
    INTERFACE
      /DEFAULTLIB:libcpmt$<$<CONFIG:Debug>:d>
  )
endif()

add_test_subdirectory(test)
