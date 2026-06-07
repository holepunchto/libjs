#include <assert.h>
#include <utf.h>
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

  js_value_t *array;
  e = js_create_array_with_length(env, 4, &array);
  assert(e == 0);

  for (uint32_t i = 0; i < 4; i++) {
    js_value_t *value;
    e = js_create_uint32(env, i * 10, &value);
    assert(e == 0);

    e = js_set_element(env, array, i, value);
    assert(e == 0);
  }

  js_value_t *elements[4];
  uint32_t read;
  e = js_get_array_elements(env, array, elements, 4, 0, &read);
  assert(e == 0);

  assert(read == 4);

  for (uint32_t i = 0; i < 4; i++) {
    uint32_t n;
    e = js_get_value_uint32(env, elements[i], &n);
    assert(e == 0);

    assert(n == i * 10);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
