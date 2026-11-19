#include <assert.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/js.h"

// Verifies that a snapshot can be taken while the environment still holds
// handles that are never serialized: references, deferreds, contexts, and
// threadsafe functions.

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_snapshot(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_ref_t *strong;
  e = js_create_reference(env, object, 1, &strong);
  assert(e == 0);

  js_ref_t *weak;
  e = js_create_reference(env, object, 0, &weak);
  assert(e == 0);

  js_deferred_t *deferred;
  js_value_t *promise;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  js_context_t *context;
  e = js_create_context(env, &context);
  assert(e == 0);

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, NULL, 0, 1, NULL, NULL, NULL, on_call, &function);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_take_snapshot(env, &data, &len);
  assert(e == 0);

  free(data);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
