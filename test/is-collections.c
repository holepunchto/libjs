#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *
eval(js_env_t *env, const char *source) {
  int e;

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) source, -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
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

  js_value_t *map = eval(env, "new Map()");
  js_value_t *set = eval(env, "new Set()");
  js_value_t *weak_map = eval(env, "new WeakMap()");
  js_value_t *weak_set = eval(env, "new WeakSet()");
  js_value_t *weak_ref = eval(env, "new WeakRef({})");
  js_value_t *map_iter = eval(env, "new Map().keys()");
  js_value_t *set_iter = eval(env, "new Set().keys()");
  js_value_t *proxy = eval(env, "new Proxy({}, {})");
  js_value_t *plain = eval(env, "({})");

  bool result;

  e = js_is_map(env, map, &result);
  assert(e == 0 && result);
  e = js_is_map(env, plain, &result);
  assert(e == 0 && !result);

  e = js_is_set(env, set, &result);
  assert(e == 0 && result);
  e = js_is_set(env, plain, &result);
  assert(e == 0 && !result);

  e = js_is_weak_map(env, weak_map, &result);
  assert(e == 0 && result);
  e = js_is_weak_map(env, map, &result);
  assert(e == 0 && !result);

  e = js_is_weak_set(env, weak_set, &result);
  assert(e == 0 && result);
  e = js_is_weak_set(env, set, &result);
  assert(e == 0 && !result);

  e = js_is_weak_ref(env, weak_ref, &result);
  assert(e == 0 && result);
  e = js_is_weak_ref(env, plain, &result);
  assert(e == 0 && !result);

  e = js_is_map_iterator(env, map_iter, &result);
  assert(e == 0 && result);
  e = js_is_map_iterator(env, plain, &result);
  assert(e == 0 && !result);

  e = js_is_set_iterator(env, set_iter, &result);
  assert(e == 0 && result);
  e = js_is_set_iterator(env, plain, &result);
  assert(e == 0 && !result);

  e = js_is_proxy(env, proxy, &result);
  assert(e == 0 && result);
  e = js_is_proxy(env, plain, &result);
  assert(e == 0 && !result);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
