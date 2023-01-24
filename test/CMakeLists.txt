list(APPEND tests
  add-finalizer
  adjust-external-memory
  atomics-wait-timeout
  atomics-wait-timeout-notify
  call-function
  call-function-ignore-return
  call-function-microtask
  call-function-microtask-nested
  call-function-throw
  create-external
  create-external-with-finalizer
  create-external-arraybuffer
  create-external-arraybuffer-with-finalizer
  create-function
  create-function-args-extra
  create-function-args-fewer
  create-function-throw
  create-function-throw-indirect
  create-function-with-ffi
  create-function-with-ffi-no-jit
  create-function-with-ffi-receiver
  create-function-with-ffi-string
  create-function-with-ffi-uint8array
  create-promise-reject
  create-promise-resolve
  create-reference
  create-reference-weak
  fatal-exception
  get-arraybuffer-info
  get-dataview-info
  get-typedarray-info-uint8array
  get-typedarray-info-uint16array
  get-value-string-utf8
  get-value-string-utf8-length
  get-value-string-utf8-no-null
  promise-rejection
  promise-rejection-unhandled
  run-module
  run-script
  run-script-ignore-return
  run-script-throw
  set-named-property
  typeof-bigint
  typeof-boolean
  typeof-external
  typeof-function
  typeof-null
  typeof-number
  typeof-object
  typeof-string
  typeof-symbol
  typeof-undefined
  wasm
  wasm-async
  wrap
  wrap-with-finalizer
  wrap-with-reference
  wrap-remove-with-finalizer
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
