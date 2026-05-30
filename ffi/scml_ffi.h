#ifndef SCML_FFI_H
#define SCML_FFI_H

#include "../vm/vm.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <vector>
extern "C" {
#endif

#define SCML_FFI_INITIAL_LIBRARIES 256
#define SCML_FFI_INITIAL_SYMBOLS 2048
#define SCML_FFI_INITIAL_SIGNATURES 1024
#define SCML_FFI_INITIAL_SEARCH_PATHS 32
#define SCML_FFI_INITIAL_STRUCTS 128
#define SCML_FFI_INITIAL_STRUCT_FIELDS 512
#define SCML_FFI_FALLBACK_MAX_ARGS 8

#define SCML_FFI_LOAD_NOW 0x01u
#define SCML_FFI_LOAD_LAZY 0x02u
#define SCML_FFI_LOAD_LOCAL 0x04u
#define SCML_FFI_LOAD_GLOBAL 0x08u

typedef enum ScmlFFIType {
    SCML_FFI_TYPE_INT32 = 0,
    SCML_FFI_TYPE_FLOAT = 1,
    SCML_FFI_TYPE_POINTER = 2,
    SCML_FFI_TYPE_VOID = 3,
    SCML_FFI_TYPE_INT64 = 4,
    SCML_FFI_TYPE_DOUBLE = 5,
    SCML_FFI_TYPE_STRING = 6,
    SCML_FFI_TYPE_INT8 = 7,
    SCML_FFI_TYPE_UINT8 = 8,
    SCML_FFI_TYPE_INT16 = 9,
    SCML_FFI_TYPE_UINT16 = 10,
    SCML_FFI_TYPE_UINT32 = 11,
    SCML_FFI_TYPE_UINT64 = 12,
    SCML_FFI_TYPE_BOOL = 13,
    SCML_FFI_TYPE_SIZE = 14
} ScmlFFIType;

typedef ScmlFFIType ScmlFFIReturnType;

typedef enum ScmlFFIAbi {
    SCML_FFI_ABI_DEFAULT = 0,
    SCML_FFI_ABI_CDECL = 1,
    SCML_FFI_ABI_STDCALL = 2,
    SCML_FFI_ABI_FASTCALL = 3,
    SCML_FFI_ABI_THISCALL = 4
} ScmlFFIAbi;

#define SCML_FFI_RET_INT SCML_FFI_TYPE_INT32
#define SCML_FFI_RET_FLOAT SCML_FFI_TYPE_FLOAT
#define SCML_FFI_RET_POINTER SCML_FFI_TYPE_POINTER
#define SCML_FFI_RET_VOID SCML_FFI_TYPE_VOID

typedef struct ScmlFFISignature {
    ScmlFFIType return_type;
    const ScmlFFIType *arg_types;
    size_t arg_count;
} ScmlFFISignature;

typedef struct ScmlFFILibrary ScmlFFILibrary;

typedef struct ScmlFFIFieldInfo {
    const char *name;
    ScmlFFIType type;
    size_t offset;
    size_t size;
    size_t alignment;
    size_t element_size;
    size_t element_count;
} ScmlFFIFieldInfo;

typedef struct ScmlFFIStats {
    size_t library_count;
    size_t library_capacity;
    size_t symbol_count;
    size_t symbol_capacity;
    size_t signature_count;
    size_t signature_capacity;
    size_t search_path_count;
    size_t search_path_capacity;
    size_t memory_block_count;
    size_t memory_block_capacity;
    size_t struct_count;
    size_t struct_capacity;
    size_t struct_field_count;
    size_t struct_field_capacity;
} ScmlFFIStats;

void *scml_ffi_load_library(const char *path);
void *scml_ffi_load_library_ex(const char *path, unsigned int flags);
int scml_ffi_add_search_path(const char *path);
int scml_ffi_unload_library(void *handle);
void *scml_ffi_get_symbol(void *library_handle, const char *function_name);
void *scml_ffi_find_symbol(const char *function_name);
int scml_ffi_call_native(void *function_ptr, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret);
int scml_ffi_call_native_ex(void *function_ptr, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret);
int scml_ffi_call_native_abi(void *function_ptr, const ScmlValue *args, const ScmlFFISignature *signature, ScmlFFIAbi abi, ScmlValue *ret);
int scml_ffi_call_vtable(void *object_ptr, size_t method_index, const ScmlValue *args, const ScmlFFISignature *signature, ScmlFFIAbi abi, int include_this, ScmlValue *ret);
void *scml_ffi_peek_pointer(const void *base, size_t index);
int scml_ffi_poke_pointer(void *base, size_t index, void *value);
int scml_ffi_call_native_by_name(const char *function_name, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret);
int scml_ffi_call_native_by_name_ex(const char *function_name, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret);
int scml_ffi_declare_function(const char *function_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count);
int scml_ffi_declare_function_text(const char *function_name, const char *return_type, const char *arg_types);
int scml_ffi_undeclare_function(const char *function_name);
const ScmlFFISignature *scml_ffi_get_declared_signature(const char *function_name);
const char *scml_ffi_last_error(void);
void scml_ffi_clear_error(void);
int scml_ffi_abi_supported(ScmlFFIAbi abi);
char *scml_ffi_capabilities(void);
ScmlFFIType scml_ffi_parse_type(const char *name, ScmlFFIType fallback);
ScmlFFIReturnType scml_ffi_parse_return_type(const char *name, ScmlFFIReturnType fallback);
ScmlFFIAbi scml_ffi_parse_abi(const char *name, ScmlFFIAbi fallback);
size_t scml_ffi_parse_arg_types(const char *spec, ScmlFFIType *out_types, size_t max_types);
void *scml_ffi_alloc(size_t size);
void *scml_ffi_alloc_array(size_t count, size_t elem_size);
void *scml_ffi_realloc_memory(void *ptr, size_t new_size);
void *scml_ffi_alloc_cstring(const char *text);
void *scml_ffi_alloc_utf16(const char *text);
char *scml_ffi_read_utf16(const void *ptr, size_t max_code_units);
char *scml_ffi_read_cstring(const void *ptr);
int scml_ffi_write_cstring(void *base, size_t offset, const char *text, size_t max_bytes);
int scml_ffi_free_memory(void *ptr);
int scml_ffi_track_memory(void *ptr, size_t size);
int scml_ffi_untrack_memory(void *ptr);
void *scml_ffi_ptr_add(void *ptr, intptr_t offset);
intptr_t scml_ffi_ptr_diff(const void *lhs, const void *rhs);
size_t scml_ffi_type_size(ScmlFFIType type);
size_t scml_ffi_type_alignment(ScmlFFIType type);
int scml_ffi_memory_read(void *base, size_t offset, ScmlFFIType type, ScmlValue *ret);
int scml_ffi_memory_write(void *base, size_t offset, ScmlFFIType type, const ScmlValue *value);
int scml_ffi_array_read(void *base, size_t index, ScmlFFIType type, ScmlValue *ret);
int scml_ffi_array_write(void *base, size_t index, ScmlFFIType type, const ScmlValue *value);
int scml_ffi_memory_copy(void *dst, const void *src, size_t size);
int scml_ffi_memory_set(void *dst, int value, size_t size);
size_t scml_ffi_memory_block_size(void *ptr);
int scml_ffi_struct_begin(const char *struct_name);
int scml_ffi_union_begin(const char *union_name);
int scml_ffi_struct_add_field(const char *struct_name, const char *field_name, ScmlFFIType type);
int scml_ffi_struct_add_array_field(const char *struct_name, const char *field_name, ScmlFFIType type, size_t element_count);
int scml_ffi_struct_finish(const char *struct_name);
int scml_ffi_struct_define_text(const char *struct_name, const char *field_spec);
size_t scml_ffi_struct_size(const char *struct_name);
size_t scml_ffi_struct_alignment(const char *struct_name);
int scml_ffi_struct_field_info(const char *struct_name, const char *field_name, ScmlFFIFieldInfo *out_info);
int scml_ffi_struct_read(void *base, const char *struct_name, const char *field_name, ScmlValue *ret);
int scml_ffi_struct_write(void *base, const char *struct_name, const char *field_name, const ScmlValue *value);
void *scml_ffi_struct_ptr(void *base, const char *struct_name, size_t index);
void *scml_ffi_struct_field_ptr(void *base, const char *struct_name, const char *field_name);
void *scml_ffi_struct_field_element_ptr(void *base, const char *struct_name, const char *field_name, size_t element_index);
int scml_ffi_struct_field_read_index(void *base, const char *struct_name, const char *field_name, size_t element_index, ScmlValue *ret);
int scml_ffi_struct_field_write_index(void *base, const char *struct_name, const char *field_name, size_t element_index, const ScmlValue *value);
void *scml_ffi_alloc_struct(const char *struct_name);
void *scml_ffi_alloc_struct_array(const char *struct_name, size_t count);
int scml_ffi_struct_array_read(void *base, size_t index, const char *struct_name, const char *field_name, ScmlValue *ret);
int scml_ffi_struct_array_write(void *base, size_t index, const char *struct_name, const char *field_name, const ScmlValue *value);
int scml_ffi_struct_undefine(const char *struct_name);
void scml_ffi_get_stats(ScmlFFIStats *out_stats);
void scml_ffi_shutdown(void);

#ifdef __cplusplus
}

inline int call_native(void *function_ptr, const std::vector<ScmlValue> &args, ScmlValue *ret) {
    return scml_ffi_call_native(function_ptr, args.data(), args.size(), SCML_FFI_TYPE_INT32, ret);
}

inline ScmlValue call_native(void *function_ptr, const std::vector<ScmlValue> &args, ScmlFFIReturnType return_type = SCML_FFI_TYPE_INT32) {
    ScmlValue ret = scml_value_int(0);
    scml_ffi_call_native(function_ptr, args.data(), args.size(), return_type, &ret);
    return ret;
}
#endif

#endif
