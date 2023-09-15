#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int keys_called = 0;

js_value_t *
own_keys (js_env_t *env, void *data) {
  int e;

  keys_called++;

  e = js_throw_error(env, NULL, "nope");
  assert(e == 0);

  return NULL;
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
    .own_keys = own_keys,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  js_value_t *value;
  e = js_get_property_names(env, delegate, &value);
  assert(e != 0);

  assert(keys_called);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(has_exception);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
