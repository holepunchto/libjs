#include <assert.h>
#include <uv.h>

#include "../include/js.h"

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

  e = js_wrap(env, object, (void *) 42, NULL, NULL, NULL);
  assert(e == 0);

  void *data = NULL;
  e = js_remove_wrap(env, object, &data);
  assert(e == 0);

  assert(data == (void *) 42);

  // The data belongs to the caller now, so the object must not hand it out
  // again.
  bool is_wrapped;
  e = js_is_wrapped(env, object, &is_wrapped);
  assert(e == 0);

  assert(!is_wrapped);

  data = NULL;
  e = js_unwrap(env, object, &data);
  assert(e != 0);

  assert(data == NULL);

  js_value_t *exception;
  e = js_get_and_clear_last_exception(env, &exception);
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
