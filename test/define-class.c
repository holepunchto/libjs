#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int constructor_called = 0;

static js_value_t *
on_construct (js_env_t *env, js_callback_info_t *info) {
  constructor_called++;

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

  js_value_t *class;
  e = js_define_class(env, "Foo", -1, on_construct, NULL, NULL, 0, &class);
  assert(e == 0);

  js_value_t *instance;
  e = js_new_instance(env, class, 0, NULL, &instance);
  assert(e == 0);

  assert(constructor_called == 1);

  bool result;
  e = js_instanceof(env, instance, class, &result);
  assert(e == 0);

  assert(result);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
