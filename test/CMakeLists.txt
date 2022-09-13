list(APPEND tests
  call-function.c
  call-function-throw.c
  create-function.c
  create-reference-weak.c
  run-module.c
  run-script.c
  set-named-property.c
)

foreach(file IN LISTS tests)
  get_filename_component(test ${file} NAME_WE)

  add_executable(${test} ${file})

  target_link_libraries(${test} js_shared)

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
