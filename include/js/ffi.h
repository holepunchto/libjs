#ifndef JS_FFI_H
#define JS_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

// This header defines an experimental FFI interface backed by the V8 fast call
// API. Use with caution!

#include <stddef.h>

#include "../js.h"

typedef struct js_ffi_type_info_s js_ffi_type_info_t;
typedef struct js_ffi_function_info_s js_ffi_function_info_t;
typedef struct js_ffi_function_s js_ffi_function_t;
typedef struct js_ffi_receiver_s js_ffi_receiver_t;
typedef struct js_ffi_string_s js_ffi_string_t;
typedef struct js_ffi_arraybuffer_s js_ffi_arraybuffer_t;
typedef struct js_ffi_typedarray_s js_ffi_typedarray_t;

typedef enum {
  js_ffi_receiver = 0,

  // Primitives
  js_ffi_void = 1,
  js_ffi_bool = 2,
  js_ffi_uint32 = 3,
  js_ffi_uint64 = 4,
  js_ffi_int32 = 5,
  js_ffi_int64 = 6,
  js_ffi_float32 = 7,
  js_ffi_float64 = 8,
  js_ffi_pointer = 9,

  // Objects
  js_ffi_string = 10,
  js_ffi_arraybuffer = 11,

  // Typed arrays
  js_ffi_uint8array = 12,
  js_ffi_uint16array = 13,
  js_ffi_uint32array = 14,
  js_ffi_uint64array = 15,
  js_ffi_int8array = 16,
  js_ffi_int16array = 17,
  js_ffi_int32array = 18,
  js_ffi_int64array = 19,
  js_ffi_float32array = 20,
  js_ffi_float64array = 21,
} js_ffi_type_t;

struct js_ffi_string_s {
  const char *data;
  uint32_t len;
};

struct js_ffi_arraybuffer_s {
  void *data;
  size_t len;
};

struct js_ffi_typedarray_s {
  size_t len;
  union {
    uint8_t *u8;
    uint16_t *u16;
    uint32_t *u32;
    uint64_t *u64;
    int8_t *i8;
    int16_t *i16;
    int32_t *i32;
    int64_t *i64;
    float *f32;
    double *f64;
  } data;
};

int
js_ffi_create_type_info(js_ffi_type_t type, js_ffi_type_info_t **result);

int
js_ffi_create_function_info(const js_ffi_type_info_t *return_info, js_ffi_type_info_t *const arg_info[], unsigned int arg_len, js_ffi_function_info_t **result);

int
js_ffi_create_function(const void *function, const js_ffi_function_info_t *type_info, js_ffi_function_t **result);

int
js_create_function_with_ffi(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_ffi_function_t *ffi, js_value_t **result);

#ifdef __cplusplus
}
#endif

#endif // JS_FFI_H
