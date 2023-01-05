#ifndef JS_H
#define JS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uv.h>

#include "js/ffi.h"

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
typedef void (*js_task_cb)(js_env_t *, void *data);
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
  bool expose_garbage_collection;
  bool disable_optimizing_compiler;
};

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

int
js_on_uncaught_exception (js_env_t *env, js_uncaught_exception_cb cb, void *data);

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
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result);

int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result);

int
js_create_string_utf8 (js_env_t *env, const char *str, size_t len, js_value_t **result);

int
js_create_object (js_env_t *env, js_value_t **result);

int
js_create_function (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result);

int
js_create_function_with_ffi (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_ffi_function_t *ffi, js_value_t **result);

int
js_create_external (js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

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
js_create_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

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
js_get_value_int32 (js_env_t *env, js_value_t *value, int32_t *result);

int
js_get_value_uint32 (js_env_t *env, js_value_t *value, uint32_t *result);

int
js_get_value_string_utf8 (js_env_t *env, js_value_t *value, char *str, size_t len, size_t *result);

int
js_get_value_external (js_env_t *env, js_value_t *value, void **result);

int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result);

int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value);

int
js_call_function (js_env_t *env, js_value_t *receiver, js_value_t *fn, size_t argc, js_value_t *const argv[], js_value_t **result);

int
js_make_callback (js_env_t *env, js_value_t *receiver, js_value_t *fn, size_t argc, js_value_t *const argv[], js_value_t **result);

int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **self, void **data);

int
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len);

int
js_get_typedarray_info (js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

int
js_throw (js_env_t *env, js_value_t *error);

int
js_throw_error (js_env_t *env, const char *code, const char *message);

int
js_is_exception_pending (js_env_t *env, bool *result);

int
js_get_and_clear_last_exception (js_env_t *env, js_value_t **result);

int
js_fatal_exception (js_env_t *env, js_value_t *error);

int
js_queue_microtask (js_env_t *env, js_task_cb cb, void *data);

int
js_queue_macrotask (js_env_t *env, js_task_cb cb, void *data, uint64_t delay);

int
js_request_garbage_collection (js_env_t *env);

#ifdef __cplusplus
}
#endif

#endif // JS_H
