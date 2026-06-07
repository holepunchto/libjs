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

  uint16_t *write;
  js_value_t *arraybuffer;
  e = js_create_arraybuffer(env, 1024 * sizeof(uint16_t), (void **) &write, &arraybuffer);
  assert(e == 0);

  write[0] = 1;
  write[512] = 2;
  write[1023] = 3;

  js_value_t *typedarray;
  e = js_create_typedarray(env, js_uint16array, 1024, arraybuffer, 0, &typedarray);
  assert(e == 0);

  js_typedarray_type_t type;
  uint16_t *data;
  size_t len;
  size_t offset;
  e = js_get_typedarray_info(env, typedarray, &type, (void **) &data, &len, NULL, &offset);
  assert(e == 0);

  assert(type == js_uint16array);
  assert(len == 1024);
  assert(offset == 0);

  assert(data[0] == 1);
  assert(data[512] == 2);
  assert(data[1023] == 3);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
