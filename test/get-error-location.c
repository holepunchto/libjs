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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "1 * 2 + ()", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);
  assert(e != 0);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_error_location_t location = {.version = 0};
  e = js_get_error_location(env, error, &location);
  assert(e == 0);

  utf8_t name[8];
  e = js_get_value_string_utf8(env, location.name, name, 8, NULL);
  assert(e == 0);

  utf8_t source[11];
  e = js_get_value_string_utf8(env, location.source, source, 11, NULL);
  assert(e == 0);

  printf(
    "name=%s\nsource=%s\nline=%lld\ncolumn=[%lld, %lld)\n",
    name,
    source,
    location.line,
    location.column_start,
    location.column_end
  );

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
