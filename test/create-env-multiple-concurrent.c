#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
run(js_env_t *env, uint32_t expected) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  char src[16];
  snprintf(src, sizeof(src), "%u", expected);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) src, -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == expected);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  // Two environments alive at the same time on the same thread.
  js_env_t *a;
  e = js_create_env(loop, platform, NULL, &a);
  assert(e == 0);

  js_env_t *b;
  e = js_create_env(loop, platform, NULL, &b);
  assert(e == 0);

  // Interleave work between the two environments.
  run(a, 11111);
  run(b, 22222);
  run(a, 33333);
  run(b, 44444);

  // Destroy out of LIFO order to stress the entered-isolate stack.
  e = js_destroy_env(a);
  assert(e == 0);

  run(b, 55555);

  e = js_destroy_env(b);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
