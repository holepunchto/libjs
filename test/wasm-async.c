#include <assert.h>
#include <uv.h>

#include "../include/js.h"

int
main (int argc, char *argv[]) {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_platform_init(loop, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(loop, platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  const char *code =
    "const code = new Uint8Array(["
    "0, 97, 115, 109, 1, 0, 0, 0, 1, 133, 128, 128, 128, 0, 1, 96, 0, 1, 127,"
    "3, 130, 128, 128, 128, 0, 1, 0, 4, 132, 128, 128, 128, 0, 1, 112, 0, 0,"
    "5, 131, 128, 128, 128, 0, 1, 0, 1, 6, 129, 128, 128, 128, 0, 0, 7, 145,"
    "128, 128, 128, 0, 2, 6, 109, 101, 109, 111, 114, 121, 2, 0, 4, 109, 97,"
    "105, 110, 0, 0, 10, 138, 128, 128, 128, 0, 1, 132, 128, 128, 128, 0, 0,"
    "65, 42, 11"
    "]);"

    "WebAssembly.instantiate(code).then((mod) => {"
    "const main = mod.instance.exports.main;"
    "return main()"
    "})";

  js_value_t *script;
  e = js_create_string_utf8(env, code, -1, &script);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_script(env, script, &promise);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  js_value_t *result;
  e = js_get_promise_result(env, promise, &result);
  assert(e == 0);

  uint32_t value;
  js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy(platform);
  assert(e == 0);
}
