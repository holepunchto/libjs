#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int uncaught_calls = 0;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  int e;

  uncaught_calls++;

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t value[8];
  e = js_get_value_string_utf8(env, message, value, 8, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "native") == 0);
}

static void
on_microtask(js_env_t *env, void *data) {
  int e;

  e = js_throw_error(env, NULL, "native");
  assert(e == 0);
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  e = js_queue_microtask_with_callback(env, on_microtask, NULL);
  assert(e == 0);

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

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  e = js_queue_microtask_with_callback(env, on_microtask, NULL);
  assert(e == 0);

  assert(uncaught_calls == 1);

  bool is_pending;
  e = js_is_exception_pending(env, &is_pending);
  assert(e == 0);

  assert(!is_pending);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(env, "queue", -1, on_call, NULL, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "queue", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "queue()", -1, &script);
  assert(e == 0);

  // The script itself succeeds, as the microtask runs at the checkpoint after
  // it rather than as part of it.
  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(uncaught_calls == 2);

  e = js_is_exception_pending(env, &is_pending);
  assert(e == 0);

  assert(!is_pending);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
