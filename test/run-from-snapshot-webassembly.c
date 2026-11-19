#include <assert.h>
#include <stdbool.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that an environment booted from a snapshot provides `WebAssembly`,
// even though the environment that produced the snapshot does not.

static void
produce(js_env_t *env, js_value_t *global) {}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "WebAssembly.validate(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]))", -1, &source);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, source, &result);
  assert(e == 0);

  bool valid;
  e = js_get_value_bool(env, result, &valid);
  assert(e == 0);

  assert(valid);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-webassembly",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
