#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int delete_called = 0;

bool
delete_property (js_env_t *env, js_value_t *property, void *data) {
  int e;

  delete_called++;

  e = js_throw_error(env, NULL, "nope");
  assert(e == 0);

  return false;
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_delegate_callbacks_t callbacks = {
    .delete_property = delete_property,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  bool result;
  e = js_delete_named_property(env, delegate, "foo", &result);
  assert(e != 0);

  assert(delete_called && !result);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(has_exception);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
