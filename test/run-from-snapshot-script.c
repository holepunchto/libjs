#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a prepared script survives an isolate snapshot: the producer
// prepares (but does not run) a script and stashes its id on the global. The
// consumer rebuilds the script from the snapshot, looks it up by id, runs it,
// and asserts both its result and its preserved name. A prepared script carries
// no callbacks or per-instance data, so nothing needs to be rebound after
// restore.

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "40 + 2", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, "survivor.js", -1, 0, source, &script);
  assert(e == 0);

  js_value_t *id;
  e = js_get_script_id(env, script, &id);
  assert(e == 0);

  e = js_set_named_property(env, global, "scriptId", id);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *id;
  e = js_get_named_property(env, global, "scriptId", &id);
  assert(e == 0);

  js_script_t *script;
  e = js_get_script_by_id(env, id, &script);
  assert(e == 0);

  const char *name;
  e = js_get_script_name(env, script, &name);
  assert(e == 0);

  assert(strcmp(name, "survivor.js") == 0);

  js_value_t *result;
  e = js_run_prepared_script(env, script, &result);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_delete_script(env, script);
  assert(e == 0);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-script",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
