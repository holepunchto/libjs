#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int get_called = 0;

js_value_t *
get (js_env_t *env, js_value_t *property, void *data) {
  int e;

  get_called++;

  utf8_t name[4];
  e = js_get_value_string_utf8(env, property, name, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) name, "foo") == 0);

  js_value_t *result;
  e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
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
    .get = get,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  bool is_delegate;
  e = js_is_delegate(env, delegate, &is_delegate);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "delegate", delegate);
  assert(e == 0);

  js_value_t *script, *result;

  e = js_create_string_utf8(env, (utf8_t *) "delegate.foo", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(get_called >= 1);

  {
    uint32_t value;
    e = js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == 42);
  }

  e = js_create_string_utf8(env, (utf8_t *) "'foo' in delegate", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(get_called >= 2);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
