#include <assert.h>
#include <stdint.h>
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

  uint64_t words_in[] = {0xdeadbeefcafebabe, 0x1234567890abcdef};

  js_value_t *value;
  e = js_create_bigint_words(env, 1, words_in, 2, &value);
  assert(e == 0);

  bool is_bigint;
  e = js_is_bigint(env, value, &is_bigint);
  assert(e == 0);

  assert(is_bigint);

  int sign;
  uint64_t words_out[4] = {0};
  size_t word_count = 4;
  e = js_get_value_bigint_words(env, value, &sign, words_out, word_count, &word_count);
  assert(e == 0);

  assert(sign == 1);
  assert(word_count == 2);
  assert(words_out[0] == words_in[0]);
  assert(words_out[1] == words_in[1]);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
