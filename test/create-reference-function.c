#include <assert.h>
#include <uv.h>

#include "../include/js.h"

js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  return NULL;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *value;
  e = js_create_function(env, "hello", -1, on_call, NULL, &value);
  assert(e == 0);

  js_ref_t *ref;
  e = js_create_reference(env, value, 1, &ref);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result != NULL);

  bool is_function;
  e = js_is_function(env, result, &is_function);
  assert(e == 0);

  assert(is_function);

  e = js_delete_reference(env, ref);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
