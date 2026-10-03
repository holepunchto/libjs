#include <assert.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that plain JavaScript state survives an isolate snapshot: the
// producer runs a script that assigns a global, and the consumer reads it back.

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.answer = 42", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *answer;
  e = js_get_named_property(env, global, "answer", &answer);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, answer, &value);
  assert(e == 0);

  assert(value == 42);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
