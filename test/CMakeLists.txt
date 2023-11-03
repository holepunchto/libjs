list(APPEND tests
  add-finalizer
  add-type-tag
  add-type-tag-delegate
  add-type-tag-external
  adjust-external-memory
  atomics-wait-timeout
  atomics-wait-timeout-notify
  call-function
  call-function-ignore-return
  call-function-microtask
  call-function-microtask-nested
  call-function-throw
  coerce-number-to-string
  coerce-object-to-string
  coerce-string-to-number
  coerce-symbol-to-string
  create-arraybuffer
  create-arraybuffer-with-backing-store
  create-date
  create-date-nan
  create-date-overflow
  create-delegate
  create-delegate-get-without-has
  create-delegate-get-throw
  create-delegate-has-throw
  create-delegate-set-throw
  create-delegate-delete-throw
  create-delegate-keys-throw
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
  create-function-with-ffi-arraybuffer
  create-function-with-ffi-finalizer
  create-function-with-ffi-no-jit
  create-function-with-ffi-pointer
  create-function-with-ffi-receiver
  create-function-with-ffi-string
  create-function-with-ffi-uint8array
  create-function-with-finalizer
  create-function-with-source
  create-module-import-missing
  create-promise-reject
  create-promise-resolve
  create-reference-array
  create-reference-bigint
  create-reference-boolean
  create-reference-function
  create-reference-null
  create-reference-number
  create-reference-object
  create-reference-object-multiple
  create-reference-object-weak
  create-reference-string
  create-reference-symbol
  create-reference-undefined
  create-sharedarraybuffer
  create-sharedarraybuffer-with-backing-store
  create-string-utf8
  create-string-utf8-with-null
  create-uint8array
  create-unsafe-arraybuffer
  define-class
  define-class-with-value
  define-class-with-static-value
  define-class-with-method
  define-class-with-static-method
  dynamic-import
  dynamic-import-without-handler
  fatal-exception
  get-arraybuffer-info
  get-dataview-info
  get-platform-identifier
  get-platform-version
  get-property-missing
  get-typedarray-info-uint8array
  get-typedarray-info-uint8array-with-offset
  get-typedarray-info-uint16array
  get-value-string-utf8
  get-value-string-utf8-length
  get-value-string-utf8-no-null
  import-meta
  import-meta-throw
  many-large-allocs
  many-small-allocs
  promise-rejection
  promise-rejection-unhandled
  run-module
  run-module-async
  run-module-cyclic-import
  run-module-nested-import
  run-module-double-nested-import
  run-module-throw
  run-script
  run-script-ignore-return
  run-script-throw
  set-named-property
  terminate-execution
  threads
  throw-error
  throw-error-formatted
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

list(APPEND skipped_tests
  # Not implemented in V8 yet
  create-function-with-ffi-arraybuffer
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c helpers.h)

  target_link_libraries(
    ${test}
    PRIVATE
      js_shared
  )

  target_include_directories(
    ${test}
    PRIVATE
      $<TARGET_PROPERTY:js,INTERFACE_INCLUDE_DIRECTORIES>
  )

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 30
  )

  add_custom_command(
    TARGET ${test}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${test}> $<TARGET_RUNTIME_DLLS:${test}>
    COMMAND_EXPAND_LISTS
  )

  if(${test} IN_LIST skipped_tests)
    set_tests_properties(
      ${test}
      PROPERTIES
      DISABLED True
    )
  endif()
endforeach()
