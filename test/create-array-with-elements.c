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

  js_value_t *first;
  e = js_create_int32(env, 1, &first);
  assert(e == 0);

  js_value_t *second;
  e = js_create_int32(env, 2, &second);
  assert(e == 0);

  js_value_t *third;
  e = js_create_int32(env, 3, &third);
  assert(e == 0);

  js_value_t *elements[] = {first, second, third};

  js_value_t *array;
  e = js_create_array_with_elements(env, elements, 3, &array);
  assert(e == 0);

  bool is_array;
  e = js_is_array(env, array, &is_array);
  assert(e == 0);

  assert(is_array);

  uint32_t len;
  e = js_get_array_length(env, array, &len);
  assert(e == 0);

  assert(len == 3);

  for (uint32_t i = 0; i < len; i++) {
    js_value_t *element;
    e = js_get_element(env, array, i, &element);
    assert(e == 0);

    int32_t value;
    e = js_get_value_int32(env, element, &value);
    assert(e == 0);

    assert(value == (int32_t) i + 1);
  }

  js_value_t *empty;
  e = js_create_array_with_elements(env, NULL, 0, &empty);
  assert(e == 0);

  e = js_get_array_length(env, empty, &len);
  assert(e == 0);

  assert(len == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
