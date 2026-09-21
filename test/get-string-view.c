#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // Concatenated at runtime so that the engine stores it as a rope, which it
  // has to flatten before the contents can be borrowed.
  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "'hello, '.repeat(8) + 'world'", -1, &script);
  assert(e == 0);

  js_value_t *string;
  e = js_run_script(env, NULL, 0, 0, script, &string);
  assert(e == 0);

  js_string_encoding_t encoding;
  const void *data;
  size_t len;

  js_string_view_t *view;
  e = js_get_string_view(env, string, &encoding, &data, &len, &view);
  assert(e == 0);

  assert(encoding == js_latin1);
  assert(len == 61);
  assert(memcmp(data, "hello, hello, hello, hello, hello, hello, hello, hello, world", 61) == 0);

  e = js_release_string_view(env, view);
  assert(e == 0);

  // The view held off garbage collection for as long as it was open, so a
  // collection is only possible again once it has been released.
  e = js_request_garbage_collection(env);
  assert(e == 0);

  js_value_t *two_byte;
  e = js_create_string_utf16le(env, (utf16_t *) u"日本語", -1, &two_byte);
  assert(e == 0);

  e = js_get_string_view(env, two_byte, &encoding, &data, &len, &view);
  assert(e == 0);

  assert(encoding == js_utf16le);
  assert(len == 3);
  assert(memcmp(data, u"日本語", 3 * sizeof(utf16_t)) == 0);

  e = js_release_string_view(env, view);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  js_value_t *empty;
  e = js_create_string_utf8(env, (utf8_t *) "", 0, &empty);
  assert(e == 0);

  e = js_get_string_view(env, empty, &encoding, &data, &len, &view);
  assert(e == 0);

  assert(len == 0);

  e = js_release_string_view(env, view);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
