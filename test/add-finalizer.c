#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool finalize_called = false;

static void
on_finalize (js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;

  assert((intptr_t) data == 42);
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
    .trace_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "({ hello: 'world' })", -1, &script);
  assert(e == 0);

  js_value_t *object;
  e = js_run_script(env, NULL, 0, 0, script, &object);
  assert(e == 0);

  e = js_add_finalizer(env, object, (void *) 42, on_finalize, NULL, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  js_request_garbage_collection(env);

  assert(finalize_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
