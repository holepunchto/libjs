#ifndef JS_FFI_H
#define JS_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

// This header defines an experimental FFI interface backed by the V8 fast call
// API. Use with caution!

#include "../js.h"

typedef struct js_ffi_type_info_s js_ffi_type_info_t;
typedef struct js_ffi_function_info_s js_ffi_function_info_t;
typedef struct js_ffi_function_s js_ffi_function_t;

typedef enum {
  js_ffi_void,
  js_ffi_bool,
  js_ffi_uint8,
  js_ffi_uint32,
  js_ffi_uint64,
  js_ffi_int32,
  js_ffi_int64,
  js_ffi_float32,
  js_ffi_float64,
} js_ffi_type_t;

typedef enum {
  js_ffi_scalar,
  js_ffi_array,
  js_ffi_typedarray,
  js_ffi_arraybuffer,
} js_ffi_kind_t;

int
js_ffi_create_type_info (js_ffi_type_t type, js_ffi_kind_t kind, js_ffi_type_info_t **result);

int
js_ffi_create_function_info (const js_ffi_type_info_t *return_info, const js_ffi_type_info_t *arg_info[], unsigned int arg_len, js_ffi_function_info_t **result);

int
js_ffi_create_function (const void *fn, const js_ffi_function_info_t *type_info, js_ffi_function_t **result);

#ifdef __cplusplus
}
#endif

#endif // JS_FFI_H
