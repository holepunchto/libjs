#include <assert.h>
#include <uv.h>

#include "../include/js.h"

js_ref_t *ref;

js_value_t *
on_call (js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *value;
  e = js_create_object(env, &value);
  assert(e == 0);

  e = js_create_reference(env, value, 0, &ref);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result != NULL);

  bool is_object;
  e = js_is_object(env, result, &is_object);
  assert(e == 0);

  assert(is_object);

  return NULL;
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_call_function(env, global, fn, 0, NULL, NULL);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result == NULL);

  e = js_delete_reference(env, ref);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
