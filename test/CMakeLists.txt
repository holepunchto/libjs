list(APPEND tests
  call-function
  call-function-throw
  create-function
  create-reference-weak
  run-macrotask
  run-macrotask-delayed
  run-microtask
  run-microtask-throw
  run-module
  run-script
  set-named-property
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c)

  target_link_libraries(${test} js_shared)

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
