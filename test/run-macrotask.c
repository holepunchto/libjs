#include <assert.h>
#include <stdbool.h>

#include "../include/js.h"

bool fn_called = false;

static void
on_call (js_env_t *env, void *data) {
  fn_called = true;
}

int
main (int argc, char *argv[]) {
  int e;

  e = js_platform_init(argv[0]);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(&env);
  assert(e == 0);

  e = js_queue_macrotask(env, on_call, NULL, 0);
  assert(e == 0);

  assert(!fn_called);

  e = js_run_macrotasks(env);
  assert(e == 0);

  assert(fn_called);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy();
  assert(e == 0);
}
