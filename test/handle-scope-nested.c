#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Nests more handle scopes than an environment keeps inline, alternating
// plain and escapable ones, and checks that every scope keeps its values until
// it closes and that escaped values survive their scope.

static uint32_t
nest(js_env_t *env, uint32_t depth, uint32_t max) {
  int e;

  if (depth == max) return 0;

  js_value_t *value;
  js_value_t *escaped = NULL;
  uint32_t sum;

  if (depth % 2 == 0) {
    js_handle_scope_t *scope;
    e = js_open_handle_scope(env, &scope);
    assert(e == 0);

    e = js_create_uint32(env, depth, &value);
    assert(e == 0);

    sum = nest(env, depth + 1, max);

    uint32_t result;
    e = js_get_value_uint32(env, value, &result);
    assert(e == 0);

    assert(result == depth);

    e = js_close_handle_scope(env, scope);
    assert(e == 0);
  } else {
    js_escapable_handle_scope_t *scope;
    e = js_open_escapable_handle_scope(env, &scope);
    assert(e == 0);

    e = js_create_uint32(env, depth, &value);
    assert(e == 0);

    sum = nest(env, depth + 1, max);

    e = js_escape_handle(env, scope, value, &escaped);
    assert(e == 0);

    e = js_close_escapable_handle_scope(env, scope);
    assert(e == 0);

    uint32_t result;
    e = js_get_value_uint32(env, escaped, &result);
    assert(e == 0);

    assert(result == depth);
  }

  return sum + depth;
}

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

  js_handle_scope_t *outer;
  e = js_open_handle_scope(env, &outer);
  assert(e == 0);

  // 0 + 1 + ... + 39
  assert(nest(env, 0, 40) == 780);

  // Once all of them have closed, the inline scopes are free again.
  assert(nest(env, 0, 40) == 780);

  e = js_close_handle_scope(env, outer);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
