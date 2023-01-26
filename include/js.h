#ifndef JS_H
#define JS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uv.h>

typedef struct js_platform_s js_platform_t;
typedef struct js_platform_options_s js_platform_options_t;
typedef struct js_env_s js_env_t;
typedef struct js_handle_scope_s js_handle_scope_t;
typedef struct js_escapable_handle_scope_s js_escapable_handle_scope_t;
typedef struct js_module_s js_module_t;
typedef struct js_value_s js_value_t;
typedef struct js_ref_s js_ref_t;
typedef struct js_deferred_s js_deferred_t;
typedef struct js_callback_info_s js_callback_info_t;

typedef js_value_t *(*js_function_cb)(js_env_t *, js_callback_info_t *);
typedef void (*js_finalize_cb)(js_env_t *, void *data, void *finalize_hint);
typedef js_module_t *(*js_module_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data);
typedef void (*js_synthetic_module_cb)(js_env_t *, js_module_t *module, void *data);
typedef void (*js_uncaught_exception_cb)(js_env_t *, js_value_t *error, void *data);
typedef void (*js_unhandled_rejection_cb)(js_env_t *, js_value_t *promise, void *data);

typedef enum {
  js_undefined,
  js_null,
  js_boolean,
  js_number,
  js_string,
  js_symbol,
  js_object,
  js_function,
  js_external,
  js_bigint,
} js_value_type_t;

typedef enum {
  js_int8_array,
  js_uint8_array,
  js_uint8_clamped_array,
  js_int16_array,
  js_uint16_array,
  js_int32_array,
  js_uint32_array,
  js_float32_array,
  js_float64_array,
  js_bigint64_array,
  js_biguint64_array,
} js_typedarray_type_t;

typedef enum {
  js_promise_pending,
  js_promise_fulfilled,
  js_promise_rejected
} js_promise_state_t;

struct js_platform_options_s {
  /**
   * Expose garbage collection APIs, which are otherwise not available as they
   * negatively impact performance.
   */
  bool expose_garbage_collection;

  /**
   * Trace invocations of the garbage collector.
   */
  bool trace_garbage_collection;

  /**
   * Disable the optimizing compiler, such as TurboFan on V8.
   */
  bool disable_optimizing_compiler;

  /**
   * Trace optimizations made by the optimizing compiler based on type feedback.
   *
   * Requires that the optimizing compiler is enabled and supports tracing.
   */
  bool trace_optimizations;

  /**
   * Trace deoptimizations made by the optimizing compiler based on type feddback.
   *
   * Requires that the optimizing compiler is enabled and supports tracing.
   */
  bool trace_deoptimizations;
};

/**
 * The identifier of the underlying JavaScript engine.
 */
extern const char *js_platform_identifier;

/**
 * The version of the underlying JavaScript engine.
 */
extern const char *js_platform_version;

int
js_create_platform (uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result);

int
js_destroy_platform (js_platform_t *platform);

int
js_get_platform_loop (js_platform_t *platform, uv_loop_t **result);

int
js_create_env (uv_loop_t *loop, js_platform_t *platform, js_env_t **result);

int
js_destroy_env (js_env_t *env);

/**
 * Add a callback for uncaught exceptions. By default, uncaught exceptions are
 * swallowed and do not affect JavaScript execution.
 */
int
js_on_uncaught_exception (js_env_t *env, js_uncaught_exception_cb cb, void *data);

/**
 * Add a callback for unhandled promise rejections. By default, unhandled promise
 * rejections are swallowed and do not affect JavaScript execution.
 *
 * A promise rejection is considered unhandled if the rejection has not been
 * caught after performing a microtask checkpoint.
 */
int
js_on_unhandled_rejection (js_env_t *env, js_unhandled_rejection_cb cb, void *data);

int
js_get_env_loop (js_env_t *env, uv_loop_t **result);

int
js_open_handle_scope (js_env_t *env, js_handle_scope_t **result);

int
js_close_handle_scope (js_env_t *env, js_handle_scope_t *scope);

int
js_open_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t **result);

int
js_close_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t *scope);

int
js_escape_handle (js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result);

int
js_run_script (js_env_t *env, js_value_t *source, js_value_t **result);

int
js_create_module (js_env_t *env, const char *name, size_t len, js_value_t *source, js_module_cb cb, void *data, js_module_t **result);

int
js_create_synthetic_module (js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t names_len, js_synthetic_module_cb cb, void *data, js_module_t **result);

int
js_delete_module (js_env_t *env, js_module_t *module);

int
js_set_module_export (js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value);

int
js_run_module (js_env_t *env, js_module_t *module, js_value_t **result);

int
js_create_reference (js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result);

int
js_delete_reference (js_env_t *env, js_ref_t *reference);

int
js_reference_ref (js_env_t *env, js_ref_t *reference, uint32_t *result);

int
js_reference_unref (js_env_t *env, js_ref_t *reference, uint32_t *result);

int
js_get_reference_value (js_env_t *env, js_ref_t *reference, js_value_t **result);

int
js_wrap (js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

int
js_unwrap (js_env_t *env, js_value_t *object, void **result);

int
js_remove_wrap (js_env_t *env, js_value_t *object, void **result);

int
js_add_finalizer (js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

int
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result);

int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result);

int
js_create_int64 (js_env_t *env, int64_t value, js_value_t **result);

int
js_create_double (js_env_t *env, double value, js_value_t **result);

int
js_create_bigint_int64 (js_env_t *env, int64_t value, js_value_t **result);

int
js_create_bigint_uint64 (js_env_t *env, uint64_t value, js_value_t **result);

int
js_create_string_utf8 (js_env_t *env, const char *str, size_t len, js_value_t **result);

int
js_create_symbol (js_env_t *env, js_value_t *description, js_value_t **result);

int
js_create_object (js_env_t *env, js_value_t **result);

int
js_create_function (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result);

int
js_create_array (js_env_t *env, js_value_t **result);

int
js_create_array_with_length (js_env_t *env, size_t len, js_value_t **result);

int
js_create_external (js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

int
js_create_date (js_env_t *env, double time, js_value_t **result);

int
js_create_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

int
js_create_type_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

int
js_create_range_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

int
js_create_syntax_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

int
js_create_promise (js_env_t *env, js_deferred_t **deferred, js_value_t **promise);

int
js_resolve_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

int
js_reject_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

int
js_get_promise_state (js_env_t *env, js_value_t *promise, js_promise_state_t *result);

int
js_get_promise_result (js_env_t *env, js_value_t *promise, js_value_t **result);

int
js_create_arraybuffer (js_env_t *env, size_t len, void **data, js_value_t **result);

int
js_create_unsafe_arraybuffer (js_env_t *env, size_t len, void **data, js_value_t **result);

int
js_create_external_arraybuffer (js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

int
js_detach_arraybuffer (js_env_t *env, js_value_t *arraybuffer);

/**
 * Toggle zero-fill of all array buffers allocated on the heap of the calling
 * thread, if supported. If disabled, `new ArrayBuffer()` is NOT guaranteed to
 * zero-fill the allocated memory.
 */
int
js_set_arraybuffer_zero_fill_enabled (bool enabled);

int
js_create_typedarray (js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

int
js_create_dataview (js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

int
js_typeof (js_env_t *env, js_value_t *value, js_value_type_t *result);

int
js_is_undefined (js_env_t *env, js_value_t *value, bool *result);

int
js_is_null (js_env_t *env, js_value_t *value, bool *result);

int
js_is_boolean (js_env_t *env, js_value_t *value, bool *result);

int
js_is_number (js_env_t *env, js_value_t *value, bool *result);

int
js_is_string (js_env_t *env, js_value_t *value, bool *result);

int
js_is_symbol (js_env_t *env, js_value_t *value, bool *result);

int
js_is_object (js_env_t *env, js_value_t *value, bool *result);

int
js_is_function (js_env_t *env, js_value_t *value, bool *result);

int
js_is_array (js_env_t *env, js_value_t *value, bool *result);

int
js_is_external (js_env_t *env, js_value_t *value, bool *result);

int
js_is_bigint (js_env_t *env, js_value_t *value, bool *result);

int
js_is_date (js_env_t *env, js_value_t *value, bool *result);

int
js_is_error (js_env_t *env, js_value_t *value, bool *result);

int
js_is_promise (js_env_t *env, js_value_t *value, bool *result);

int
js_is_arraybuffer (js_env_t *env, js_value_t *value, bool *result);

int
js_is_detached_arraybuffer (js_env_t *env, js_value_t *value, bool *result);

int
js_is_typedarray (js_env_t *env, js_value_t *value, bool *result);

int
js_is_dataview (js_env_t *env, js_value_t *value, bool *result);

int
js_strict_equals (js_env_t *env, js_value_t *a, js_value_t *b, bool *result);

int
js_get_global (js_env_t *env, js_value_t **result);

int
js_get_undefined (js_env_t *env, js_value_t **result);

int
js_get_null (js_env_t *env, js_value_t **result);

int
js_get_boolean (js_env_t *env, bool value, js_value_t **result);

int
js_get_value_bool (js_env_t *env, js_value_t *value, bool *result);

int
js_get_value_int32 (js_env_t *env, js_value_t *value, int32_t *result);

int
js_get_value_uint32 (js_env_t *env, js_value_t *value, uint32_t *result);

int
js_get_value_int64 (js_env_t *env, js_value_t *value, int64_t *result);

int
js_get_value_double (js_env_t *env, js_value_t *value, double *result);

int
js_get_value_bigint_int64 (js_env_t *env, js_value_t *value, int64_t *result);

int
js_get_value_bigint_uint64 (js_env_t *env, js_value_t *value, uint64_t *result);

int
js_get_value_string_utf8 (js_env_t *env, js_value_t *value, char *str, size_t len, size_t *result);

int
js_get_value_external (js_env_t *env, js_value_t *value, void **result);

int
js_get_value_date (js_env_t *env, js_value_t *value, double *result);

int
js_get_array_length (js_env_t *env, js_value_t *value, uint32_t *result);

int
js_get_property (js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result);

int
js_has_property (js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

int
js_set_property (js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value);

int
js_delete_property (js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result);

int
js_has_named_property (js_env_t *env, js_value_t *object, const char *name, bool *result);

int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value);

int
js_delete_named_property (js_env_t *env, js_value_t *object, const char *name, bool *result);

int
js_get_element (js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result);

int
js_has_element (js_env_t *env, js_value_t *object, uint32_t index, bool *result);

int
js_set_element (js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value);

int
js_delete_element (js_env_t *env, js_value_t *object, uint32_t index, bool *result);

int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data);

int
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len);

int
js_get_typedarray_info (js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

/**
 * Call a JavaScript function from native code.
 *
 * When there is no JavaScript already executing on the stack, such as when the
 * native code making the call was invoked as the result of I/O, a microtask
 * checkpoint is performed before returning to native code.
 *
 * If there is JavaScript already executing on the stack, such as when the
 * native code making the call was invoked from JavaScript, no microtask
 * checkpoint is performed before returning to native code.
 */
int
js_call_function (js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result);

int
js_throw (js_env_t *env, js_value_t *error);

int
js_throw_error (js_env_t *env, const char *code, const char *message);

int
js_throw_verrorf (js_env_t *env, const char *code, const char *message, va_list args);

static inline int
js_throw_errorf (js_env_t *env, const char *code, const char *message, ...) {
  va_list args;
  va_start(args, message);

  return js_throw_verrorf(env, code, message, args);
}

int
js_throw_type_error (js_env_t *env, const char *code, const char *message);

int
js_throw_type_verrorf (js_env_t *env, const char *code, const char *message, va_list args);

static inline int
js_throw_type_errorf (js_env_t *env, const char *code, const char *message, ...) {
  va_list args;
  va_start(args, message);

  return js_throw_type_verrorf(env, code, message, args);
}

int
js_throw_range_error (js_env_t *env, const char *code, const char *message);

int
js_throw_range_verrorf (js_env_t *env, const char *code, const char *message, va_list args);

static inline int
js_throw_range_errorf (js_env_t *env, const char *code, const char *message, ...) {
  va_list args;
  va_start(args, message);

  return js_throw_range_verrorf(env, code, message, args);
}

int
js_throw_syntax_error (js_env_t *env, const char *code, const char *message);

int
js_throw_syntax_verrorf (js_env_t *env, const char *code, const char *message, va_list args);

static inline int
js_throw_syntax_errorf (js_env_t *env, const char *code, const char *message, ...) {
  va_list args;
  va_start(args, message);

  return js_throw_syntax_verrorf(env, code, message, args);
}

int
js_is_exception_pending (js_env_t *env, bool *result);

int
js_get_and_clear_last_exception (js_env_t *env, js_value_t **result);

/**
 * Trigger an uncaught exception. If no uncaught exception handler is installed,
 * the function has no effect and execution will continue normally.
 */
int
js_fatal_exception (js_env_t *env, js_value_t *error);

int
js_adjust_external_memory (js_env_t *env, int64_t change_in_bytes, int64_t *result);

/**
 * Request that the garbage collector be run. This should only be used for
 * testing as it will negatively impact performance.
 *
 * Requires that the `expose_garbage_collection` option is `true`.
 */
int
js_request_garbage_collection (js_env_t *env);

#ifdef __cplusplus
}
#endif

#endif // JS_H
