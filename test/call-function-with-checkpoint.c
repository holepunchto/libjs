#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool resolved = false;

static js_value_t *
on_resolved(js_env_t *env, js_callback_info_t *info) {
  resolved = true;
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "(fn) => Promise.resolve().then(fn)", -1, &source);
  assert(e == 0);

  js_value_t *factory;
  e = js_run_script(env, NULL, 0, 0, source, &factory);
  assert(e == 0);

  js_value_t *cb;
  e = js_create_function(env, "cb", -1, on_resolved, NULL, &cb);
  assert(e == 0);

  js_value_t *receiver;
  e = js_get_undefined(env, &receiver);
  assert(e == 0);

  js_value_t *args[] = {cb};

  js_value_t *result;
  e = js_call_function_with_checkpoint(env, receiver, factory, 1, args, &result);
  assert(e == 0);

  assert(resolved);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
