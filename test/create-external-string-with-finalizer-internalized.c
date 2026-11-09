#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Using an external string as a property key whose name is already
// internalized makes the engine release the string right away, while it holds
// off garbage collection. The finalizer must only run once that is over, so
// that it may call into the environment.

static uint32_t finalize_count = 0;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  finalize_count++;
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

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_value_t *value;
  e = js_get_null(env, &value);
  assert(e == 0);

  e = js_set_named_property(env, object, "foo", value);
  assert(e == 0);

  latin1_t data[] = {'f', 'o', 'o'};

  js_value_t *key;
  e = js_create_external_string_latin1(env, data, 3, on_finalize, NULL, &key, NULL);
  assert(e == 0);

  e = js_set_property(env, object, key, value);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_count == 1);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
