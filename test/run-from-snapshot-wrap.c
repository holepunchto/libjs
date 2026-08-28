#include <assert.h>
#include <stdint.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a `js_wrap`'d object survives an isolate snapshot: the producer
// wraps an object reachable from the global with non-NULL data and a finalizer.
// After restore its finalizer side-table slot is rebuilt and the data rebound,
// so `js_unwrap` returns it.

static void
on_wrap_finalize(js_env_t *env, void *data, void *finalize_hint) {}

static bool
rebind(js_env_t *env, const js_rebind_info_t *info, void *context, js_binding_payload_t *result) {
  if (info->type == js_binding_finalizer && info->finalizer.cb == on_wrap_finalize) {
    result->data = (void *) (intptr_t) 99;

    return true;
  }

  return false;
}

static void
references(js_external_references_t *references) {
  int e;

  e = js_add_external_reference(references, (const void *) on_wrap_finalize);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *wrapped;
  e = js_create_object(env, &wrapped);
  assert(e == 0);

  e = js_wrap(env, wrapped, (void *) (intptr_t) 99, on_wrap_finalize, NULL, NULL);
  assert(e == 0);

  e = js_set_named_property(env, global, "wrapped", wrapped);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *wrapped;
  e = js_get_named_property(env, global, "wrapped", &wrapped);
  assert(e == 0);

  void *data;
  e = js_unwrap(env, wrapped, &data);
  assert(e == 0);

  assert((int32_t) (intptr_t) data == 99);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-wrap",
    .references = references,
    .rebind = rebind,
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
