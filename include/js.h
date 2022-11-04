#ifndef JS_H
#define JS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct js_env_s js_env_t;
typedef struct js_handle_scope_s js_handle_scope_t;
typedef struct js_escapable_handle_scope_s js_escapable_handle_scope_t;
typedef struct js_module_s js_module_t;
typedef struct js_value_s js_value_t;
typedef struct js_ref_s js_ref_t;
typedef struct js_deferred_s js_deferred_t;
typedef struct js_callback_info_s js_callback_info_t;
typedef struct js_task_s js_task_t;

typedef js_value_t *(*js_function_cb)(js_env_t *, const js_callback_info_t *);
typedef js_module_t *(*js_module_resolve_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data);
typedef js_value_t *(*js_synethic_module_cb)(js_env_t *, js_module_t *module, void *data);
typedef void (*js_task_cb)(js_env_t *, void *data);

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

int
js_platform_init (const char *path);

int
js_platform_destroy ();

int
js_set_flags_from_string (const char *string, size_t len);

int
js_set_flags_from_command_line (int *argc, char **argv, bool remove_flags);

int
js_env_init (js_env_t **result);

int
js_env_destroy (js_env_t *env);

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
js_create_module (js_env_t *env, const char *name, size_t len, js_value_t *source, void *data, js_module_t **result);

int
js_create_synthetic_module (js_env_t *env, const char *name, size_t len, const js_value_t *export_names[], size_t names_len, js_synethic_module_cb cb, void *data, js_module_t **result);

int
js_delete_module (js_env_t *env, js_module_t *module);

int
js_set_module_export (js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value);

int
js_instantiate_module (js_env_t *env, js_module_t *module, js_module_resolve_cb cb);

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
js_create_promise (js_env_t *env, js_deferred_t **deferred, js_value_t **promise);

int
js_resolve_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

int
js_reject_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

int
js_typeof (js_env_t *env, js_value_t *value, js_value_type_t *result);

int
js_is_array (js_env_t *env, js_value_t *value, bool *result);

int
js_is_arraybuffer (js_env_t *env, js_value_t *value, bool *result);

int
js_is_number (js_env_t *env, js_value_t *value, bool *result);

int
js_is_bigint (js_env_t *env, js_value_t *value, bool *result);

int
js_is_null (js_env_t *env, js_value_t *value, bool *result);

int
js_is_undefined (js_env_t *env, js_value_t *value, bool *result);

int
js_is_symbol (js_env_t *env, js_value_t *value, bool *result);

int
js_is_boolean (js_env_t *env, js_value_t *value, bool *result);

int
js_is_external (js_env_t *env, js_value_t *value, bool *result);

int
js_is_string (js_env_t *env, js_value_t *value, bool *result);

int
js_is_function (js_env_t *env, js_value_t *value, bool *result);

int
js_is_object (js_env_t *env, js_value_t *value, bool *result);

int
js_is_date (js_env_t *env, js_value_t *value, bool *result);

int
js_is_error (js_env_t *env, js_value_t *value, bool *result);

int
js_is_typedarray (js_env_t *env, js_value_t *value, bool *result);

int
js_is_dataview (js_env_t *env, js_value_t *value, bool *result);

int
js_is_promise (js_env_t *env, js_value_t *value, bool *result);

int
js_strict_equals (js_env_t *env, js_value_t *a, js_value_t *b, bool *result);

int
js_get_global (js_env_t *env, js_value_t **result);

int
js_get_null (js_env_t *env, js_value_t **result);

int
js_get_undefined (js_env_t *env, js_value_t **result);

int
js_get_boolean (js_env_t *env, bool value, js_value_t **result);

int
js_get_value_int32 (js_env_t *env, js_value_t *value, int32_t *result);

int
js_get_value_uint32 (js_env_t *env, js_value_t *value, uint32_t *result);

int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result);

int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value);

int
js_call_function (js_env_t *env, js_value_t *recv, js_value_t *fn, size_t argc, const js_value_t *argv[], js_value_t **result);

int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t *self, void **data);

int
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len);

int
js_get_typedarray_info (js_env_t env, js_value_t *typedarray, js_typedarray_type_t *type, size_t *len, void **data, js_value_t **arraybuffer, size_t *offset);

int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, size_t *len, void **data, js_value_t **arraybuffer, size_t *offset);

int
js_throw (js_env_t *env, js_value_t *error);

int
js_queue_microtask (js_env_t *env, js_task_cb cb, void *data);

int
js_run_microtasks (js_env_t *env);

int
js_queue_macrotask (js_env_t *env, js_task_cb cb, void *data);

int
js_run_macrotasks (js_env_t *env);

int
js_request_garbage_collection (js_env_t *env);

#ifdef __cplusplus
}
#endif
#endif // JS_H
