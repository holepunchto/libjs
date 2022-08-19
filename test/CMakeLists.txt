list(APPEND tests
  run-module
  run-script
)

foreach(test IN LISTS tests)
  add_executable(test_${test} ${test}.c)

  target_link_libraries(test_${test} js_static)

  add_test(
    NAME ${test}
    COMMAND test_${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
