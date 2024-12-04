#ifndef JS_TEST_HELPERS_H
#define JS_TEST_HELPERS_H

#include <assert.h>
#include <stdio.h>
#include <utf.h>

#include "../include/js.h"

static inline void
js_print_pending_exception(js_env_t *env) {
  int err;

  js_value_t *exception;
  err = js_get_and_clear_last_exception(env, &exception);
  assert(err == 0);

  js_value_t *string;
  err = js_coerce_to_string(env, exception, &string);
  assert(err == 0);

  utf8_t message[1024];
  err = js_get_value_string_utf8(env, string, message, 1024, NULL);
  assert(err == 0);

  printf("err=%s\n", message);
}

#endif // JS_TEST_HELPERS_H
