#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"

#define len 5000

#define survivors 10

static js_ref_t *refs[len];

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // Enough references to span several segments.
  for (int32_t i = 0; i < len; i++) {
    js_value_t *value;
    e = js_create_int32(env, i, &value);
    assert(e == 0);

    e = js_create_reference(env, value, 1, &refs[i]);
    assert(e == 0);
  }

  // Releasing all but a handful of the first leaves the environment free to
  // hand back everything it has outgrown when it next collects.
  for (int32_t i = survivors; i < len; i++) {
    e = js_delete_reference(env, refs[i]);
    assert(e == 0);
  }

  e = js_request_garbage_collection(env);
  assert(e == 0);

  for (int32_t i = 0; i < survivors; i++) {
    js_value_t *value;
    e = js_get_reference_value(env, refs[i], &value);
    assert(e == 0);

    assert(value != NULL);

    int32_t actual;
    e = js_get_value_int32(env, value, &actual);
    assert(e == 0);

    assert(actual == i);
  }

  // The references that survived must not have moved, and the ones taken out
  // again must come back intact.
  for (int32_t i = survivors; i < len; i++) {
    js_value_t *value;
    e = js_create_int32(env, i, &value);
    assert(e == 0);

    e = js_create_reference(env, value, 1, &refs[i]);
    assert(e == 0);
  }

  for (int32_t i = 0; i < len; i++) {
    js_value_t *value;
    e = js_get_reference_value(env, refs[i], &value);
    assert(e == 0);

    assert(value != NULL);

    int32_t actual;
    e = js_get_value_int32(env, value, &actual);
    assert(e == 0);

    assert(actual == i);
  }

  for (int32_t i = 0; i < len; i++) {
    e = js_delete_reference(env, refs[i]);
    assert(e == 0);
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
