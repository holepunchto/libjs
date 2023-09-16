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
  e = js_create_string_utf8(env, (utf8_t *) "Uint8Array.from([1, 2, 3, 4, 5, 6]).subarray(2)", -1, &script);
  assert(e == 0);

  js_value_t *typedarray;
  e = js_run_script(env, NULL, 0, 0, script, &typedarray);
  assert(e == 0);

  js_typedarray_type_t type;
  uint8_t *data;
  size_t len;
  js_value_t *arraybuffer;
  size_t offset;
  e = js_get_typedarray_info(env, typedarray, &type, (void **) &data, &len, &arraybuffer, &offset);
  assert(e == 0);

  assert(type == js_uint8_array);
  assert(len == 4);
  assert(offset == 2);

  assert(data[0] == 3);
  assert(data[1] == 4);
  assert(data[2] == 5);
  assert(data[3] == 6);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
