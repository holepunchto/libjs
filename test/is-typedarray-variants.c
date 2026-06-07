#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *
eval(js_env_t *env, const char *source) {
  int e;

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) source, -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  return result;
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *i8 = eval(env, "new Int8Array(4)");
  js_value_t *u8 = eval(env, "new Uint8Array(4)");
  js_value_t *u8c = eval(env, "new Uint8ClampedArray(4)");
  js_value_t *i16 = eval(env, "new Int16Array(4)");
  js_value_t *u16 = eval(env, "new Uint16Array(4)");
  js_value_t *i32 = eval(env, "new Int32Array(4)");
  js_value_t *u32 = eval(env, "new Uint32Array(4)");
  js_value_t *f32 = eval(env, "new Float32Array(4)");
  js_value_t *f64 = eval(env, "new Float64Array(4)");
  js_value_t *b64 = eval(env, "new BigInt64Array(4)");
  js_value_t *bu64 = eval(env, "new BigUint64Array(4)");

  bool result;

#define ASSERT_ONLY(predicate, only)                       \
  {                                                        \
    js_value_t *cases[] = {                                \
      i8, u8, u8c, i16, u16, i32, u32, f32, f64, b64, bu64 \
    };                                                     \
    for (size_t i = 0; i < 11; i++) {                      \
      e = predicate(env, cases[i], &result);               \
      assert(e == 0);                                      \
      assert(result == (cases[i] == only));                \
    }                                                      \
  }

  ASSERT_ONLY(js_is_int8array, i8);
  ASSERT_ONLY(js_is_uint8array, u8);
  ASSERT_ONLY(js_is_uint8clampedarray, u8c);
  ASSERT_ONLY(js_is_int16array, i16);
  ASSERT_ONLY(js_is_uint16array, u16);
  ASSERT_ONLY(js_is_int32array, i32);
  ASSERT_ONLY(js_is_uint32array, u32);
  ASSERT_ONLY(js_is_float32array, f32);
  ASSERT_ONLY(js_is_float64array, f64);
  ASSERT_ONLY(js_is_bigint64array, b64);
  ASSERT_ONLY(js_is_biguint64array, bu64);

#undef ASSERT_ONLY

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
