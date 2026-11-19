#include <assert.h>
#include <stdbool.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that an object created while warming up belongs to the restored
// default context: the producer stores an object on the global, and the
// consumer asserts that the object's global is the restored global.

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_set_named_property(env, global, "object", object);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *object;
  e = js_get_named_property(env, global, "object", &object);
  assert(e == 0);

  js_value_t *result;
  e = js_get_object_global(env, object, &result);
  assert(e == 0);

  bool equals;
  e = js_strict_equals(env, result, global, &equals);
  assert(e == 0);

  assert(equals);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-object-global",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
