#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool called = false;

static js_value_t *
on_microtask(js_env_t *env, js_callback_info_t *info) {
  called = true;

  return NULL;
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *fn;
  e = js_create_function(env, "f", -1, on_microtask, NULL, &fn);
  assert(e == 0);

  e = js_queue_microtask(env, fn);
  assert(e == 0);

  // The microtask has only been queued and so has not run yet, as there is
  // still JavaScript executing on the stack.
  assert(called == false);

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

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  // Expose a function that queues the microtask so that it can be queued while
  // JavaScript is executing on the stack.
  js_value_t *fn;
  e = js_create_function(env, "queue", -1, on_call, NULL, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "queue", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "queue()", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  // The script has finished executing and so a microtask checkpoint has been
  // performed, draining the queued microtask.
  assert(called == true);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
