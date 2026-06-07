#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool was_construct = false;
static bool was_call = false;

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *new_target;
  e = js_get_new_target(env, info, &new_target);
  assert(e == 0);

  bool is_undefined;
  e = js_is_undefined(env, new_target, &is_undefined);
  assert(e == 0);

  if (is_undefined) {
    was_call = true;
  } else {
    was_construct = true;
  }

  js_value_t *result;
  e = js_create_object(env, &result);
  assert(e == 0);

  return result;
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

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *receiver;
  e = js_create_object(env, &receiver);
  assert(e == 0);

  js_value_t *direct_result;
  e = js_call_function(env, receiver, fn, 0, NULL, &direct_result);
  assert(e == 0);

  js_value_t *instance;
  e = js_new_instance(env, fn, 0, NULL, &instance);
  assert(e == 0);

  assert(was_call);
  assert(was_construct);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
