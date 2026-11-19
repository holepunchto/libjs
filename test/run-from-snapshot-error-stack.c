#include <assert.h>
#include <stdbool.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that errors created in an environment booted from a snapshot have
// stack traces, even though errors created in the environment that produced
// the snapshot do not.

static void
produce(js_env_t *env, js_value_t *global) {}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "(function inner() { return new Error().stack })().indexOf('inner') !== -1", -1, &source);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, source, &result);
  assert(e == 0);

  bool has_frame;
  e = js_get_value_bool(env, result, &has_frame);
  assert(e == 0);

  assert(has_frame);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-error-stack",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
