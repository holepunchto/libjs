#include <assert.h>
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
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "new DataView(Uint8Array.from([1, 2, 3, 4]).buffer)", -1, &script);
  assert(e == 0);

  js_value_t *dataview;
  e = js_run_script(env, script, &dataview);
  assert(e == 0);

  uint8_t *data;
  size_t len;
  js_value_t *arraybuffer;
  size_t offset;
  e = js_get_dataview_info(env, dataview, (void **) &data, &len, &arraybuffer, &offset);
  assert(e == 0);

  assert(len == 4);
  assert(offset == 0);

  assert(data[0] == 1);
  assert(data[1] == 2);
  assert(data[2] == 3);
  assert(data[3] == 4);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
