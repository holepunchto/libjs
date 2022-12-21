list(APPEND tests
  atomics-wait-timeout
  atomics-wait-timeout-notify
  call-function
  call-function-throw
  create-function
  create-function-with-ffi
  create-reference
  create-reference-weak
  run-macrotask
  run-macrotask-delayed
  run-microtask
  run-microtask-throw
  run-module
  run-script
  set-named-property
  wasm
  wasm-async
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c)

  target_link_libraries(
    ${test}
    PRIVATE
      js_shared
  )

  target_include_directories(
    ${test}
    PRIVATE
      $<TARGET_PROPERTY:js,INCLUDE_DIRECTORIES>
  )

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
