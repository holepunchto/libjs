#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "({ hello: 'world' })", -1, &script);
  assert(e == 0);

  js_value_t *object;
  e = js_run_script(env, NULL, 0, 0, script, &object);
  assert(e == 0);

  e = js_wrap(env, object, (void *) 42, NULL, NULL, NULL);
  assert(e == 0);

  bool is_wrapped;
  e = js_is_wrapped(env, object, &is_wrapped);
  assert(e == 0);

  assert(is_wrapped);

  intptr_t value;
  e = js_unwrap(env, object, (void **) &value);
  assert(e == 0);

  assert(value == 42);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
