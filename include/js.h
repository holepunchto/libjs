#ifndef JS_H
#define JS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct js_env_s js_env_t;
typedef struct js_handle_scope_s js_handle_scope_t;
typedef struct js_value_s js_value_t;
typedef struct js_callback_info_s js_callback_info_t;

typedef js_value_t *(*js_callback_t)(js_env_t *, const js_callback_info_t *);

int
js_platform_init (const char *path);

int
js_platform_destroy ();

int
js_env_init (js_env_t **result);

int
js_env_destroy (js_env_t *env);

int
js_run_script (js_env_t *env, js_value_t *script, js_value_t **result);

int
js_run_module (js_env_t *env, js_value_t *module, const char *name, js_value_t **result);

int
js_open_handle_scope (js_env_t *env, js_handle_scope_t **result);

int
js_close_handle_scope (js_env_t *env, js_handle_scope_t *scope);

int
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result);

int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result);

int
js_create_string_utf8 (js_env_t *env, const char *str, size_t len, js_value_t **result);

int
js_create_function (js_env_t *env, const char *name, size_t len, js_callback_t cb, void *data, js_value_t **result);

int
js_get_global (js_env_t *env, js_value_t **result);

int
js_get_null (js_env_t *env, js_value_t **result);

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

#ifdef __cplusplus
}
#endif
#endif // JS_H
