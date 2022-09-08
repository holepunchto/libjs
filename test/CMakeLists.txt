list(APPEND tests
  create-function
  run-module
  run-script
  set-named-property
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c)

  target_link_libraries(${test} js_static)

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
