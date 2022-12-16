#include <vector>

#include <v8-fast-api-calls.h>
#include <v8.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

using namespace v8;

int
js_ffi_create_type_info (js_ffi_type_t type, js_ffi_kind_t kind, js_ffi_type_info_t **result) {
  CTypeInfo::Type v8_type;
  CTypeInfo::SequenceType v8_sequence_type;

  switch (type) {
  case js_ffi_void:
    v8_type = CTypeInfo::Type::kVoid;
    break;
  case js_ffi_bool:
    v8_type = CTypeInfo::Type::kBool;
    break;
  case js_ffi_uint8:
    v8_type = CTypeInfo::Type::kUint8;
    break;
  case js_ffi_uint32:
    v8_type = CTypeInfo::Type::kUint32;
    break;
  case js_ffi_uint64:
    v8_type = CTypeInfo::Type::kUint64;
    break;
  case js_ffi_int32:
    v8_type = CTypeInfo::Type::kInt32;
    break;
  case js_ffi_int64:
    v8_type = CTypeInfo::Type::kInt64;
    break;
  case js_ffi_float32:
    v8_type = CTypeInfo::Type::kFloat32;
    break;
  case js_ffi_float64:
    v8_type = CTypeInfo::Type::kFloat64;
    break;
  }

  switch (kind) {
  case js_ffi_scalar:
    v8_sequence_type = CTypeInfo::SequenceType::kScalar;
    break;
  case js_ffi_array:
    v8_sequence_type = CTypeInfo::SequenceType::kIsSequence;
    break;
  case js_ffi_typedarray:
    v8_sequence_type = CTypeInfo::SequenceType::kIsTypedArray;
    break;
  case js_ffi_arraybuffer:
    v8_sequence_type = CTypeInfo::SequenceType::kIsArrayBuffer;
    break;
  }

  auto type_info = new CTypeInfo(v8_type, v8_sequence_type);

  *result = reinterpret_cast<js_ffi_type_info_t *>(type_info);

  return 0;
}

int
js_ffi_create_function_info (const js_ffi_type_info_t *return_info, const js_ffi_type_info_t *arg_info[], unsigned int arg_len, js_ffi_function_info_t **result) {
  auto v8_return_info = reinterpret_cast<const CTypeInfo &>(*return_info);

  std::vector<CTypeInfo> v8_arg_info;

  for (unsigned int i = 0; i < arg_len; i++) {
    v8_arg_info.push_back(*reinterpret_cast<const CTypeInfo *>(arg_info[i]));
  }

  auto function_info = new CFunctionInfo(v8_return_info, arg_len, v8_arg_info.data());

  *result = reinterpret_cast<js_ffi_function_info_t *>(function_info);

  return 0;
}

int
js_ffi_create_function (const void *fn, const js_ffi_function_info_t *type_info, js_ffi_function_t **result) {
  auto v8_type_info = reinterpret_cast<const CFunctionInfo *>(type_info);

  auto function = new CFunction(fn, v8_type_info);

  *result = reinterpret_cast<js_ffi_function_t *>(function);

  return 0;
}
