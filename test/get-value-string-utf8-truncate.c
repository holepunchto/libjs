#include <assert.h>
#include <string.h>
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

  js_value_t *string;
  e = js_create_string_utf8(env, (utf8_t *) "hello", -1, &string);
  assert(e == 0);

  // The guard catches a conversion that writes the whole string rather than as
  // much of it as was asked for.
  struct {
    utf8_t value[3];
    utf8_t guard[8];
  } buffer;

  memset(buffer.guard, 0xaa, sizeof(buffer.guard));

  size_t written;
  e = js_get_value_string_utf8(env, string, buffer.value, sizeof(buffer.value), &written);
  assert(e == 0);

  assert(memcmp(buffer.value, "hel", 3) == 0);
  assert(written == 3);

  for (size_t i = 0; i < sizeof(buffer.guard); i++) {
    assert(buffer.guard[i] == 0xaa);
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
