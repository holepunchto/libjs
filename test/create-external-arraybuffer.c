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
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  uint8_t data[] = {1, 2, 3, 4};

  js_value_t *arraybuffer;
  e = js_create_external_arraybuffer(env, data, 4, NULL, NULL, &arraybuffer);

  if (e == 0) {
    js_value_t *global;
    e = js_get_global(env, &global);
    assert(e == 0);

    e = js_set_named_property(env, global, "arraybuffer", arraybuffer);
    assert(e == 0);

    js_value_t *script;
    e = js_create_string_utf8(env, (utf8_t *) "const view = new Uint8Array(arraybuffer); view[0] = 42", -1, &script);
    assert(e == 0);

    e = js_run_script(env, NULL, 0, 0, script, NULL);
    assert(e == 0);

    assert(data[0] == 42);
  }

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
