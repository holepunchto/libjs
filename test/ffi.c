#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

uint8_t
on_call (uint8_t arg) {
  return arg >> 1;
}

int
main () {
  js_ffi_type_info_t *return_info;
  js_ffi_create_type_info(js_ffi_uint8, js_ffi_scalar, &return_info);

  js_ffi_type_info_t *arg_info;
  js_ffi_create_type_info(js_ffi_uint8, js_ffi_scalar, &arg_info);

  js_ffi_function_info_t *function_info;
  js_ffi_create_function_info(return_info, &arg_info, 1, &function_info);

  js_ffi_function_t *function;
  js_ffi_create_function(on_call, function_info, &function);
}
