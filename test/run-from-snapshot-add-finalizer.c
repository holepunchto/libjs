#include <assert.h>
#include <stdbool.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that an object with a finalizer added by `js_add_finalizer()` does
// not prevent the environment holding it from being serialized: the producer
// adds a finalizer to an object on the global, and the consumer asserts that
// the object survived.

static void
finalize(js_env_t *env, void *data, void *finalize_hint) {}

static void
references(js_external_references_t *references) {
  int e;

  e = js_add_external_reference(references, (const void *) finalize);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_add_finalizer(env, object, NULL, finalize, NULL, NULL);
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

  bool is_object;
  e = js_is_object(env, object, &is_object);
  assert(e == 0);

  assert(is_object);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-add-finalizer",
    .references = references,
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
