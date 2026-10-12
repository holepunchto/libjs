#include <assert.h>
#include <stdint.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a delegate survives an isolate snapshot: the producer creates a
// delegate reachable from the global. After restore its internal field is
// reconstructed and its data rebound, so property access routes through the
// rebound getter, which returns its data.

static js_value_t *
delegate_get(js_env_t *env, js_value_t *property, void *data) {
  int e;

  js_value_t *result;
  e = js_create_int32(env, (int32_t) (intptr_t) data, &result);
  assert(e == 0);

  return result;
}

static bool
rebind(js_env_t *env, const js_rebind_info_t *info, void *context, js_binding_payload_t *result) {
  if (info->type == js_binding_delegate && info->delegate.callbacks.get == delegate_get) {
    result->data = (void *) (intptr_t) 123;

    return true;
  }

  return false;
}

static void
references(js_external_references_t *references) {
  int e;

  e = js_add_external_reference(references, (const void *) delegate_get);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_delegate_callbacks_t callbacks = {
    .version = 0,
    .get = delegate_get,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, (void *) (intptr_t) 123, NULL, NULL, &delegate);
  assert(e == 0);

  e = js_set_named_property(env, global, "delegate", delegate);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *delegate;
  e = js_get_named_property(env, global, "delegate", &delegate);
  assert(e == 0);

  js_value_t *delegated;
  e = js_get_named_property(env, delegate, "anything", &delegated);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, delegated, &value);
  assert(e == 0);

  assert(value == 123);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-delegate",
    .references = references,
    .rebind = rebind,
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
