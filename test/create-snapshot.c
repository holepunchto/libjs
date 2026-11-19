#include <assert.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Validates the snapshot producer: warm up an environment, serialize it, and
// confirm a non-empty blob is handed back and the environment tears down
// cleanly.
//
// The blob is deliberately not booted in this same process: this build enables
// V8's shared read-only heap (a build-level setting), so loading a freshly
// produced blob in a process that already has a shared read-only heap trips a
// debug-only checksum CHECK. Snapshots are produced offline and consumed in a
// separate process, so full round-trip coverage belongs to a release or
// cross-process test.

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_snapshot(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.answer = 42", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_take_snapshot(env, &data, &len);
  assert(e == 0);

  // `env` is consumed by `js_take_snapshot()` and must not be destroyed.

  assert(data != NULL);
  assert(len > 0);

  free(data);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
