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
    SCML_FFI_TYPE_STRING = 6
} ScmlFFIType;

typedef ScmlFFIType ScmlFFIReturnType;

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
} ScmlFFIStats;

void *scml_ffi_load_library(const char *path);
void *scml_ffi_load_library_ex(const char *path, unsigned int flags);
int scml_ffi_add_search_path(const char *path);
int scml_ffi_unload_library(void *handle);
void *scml_ffi_get_symbol(void *library_handle, const char *function_name);
void *scml_ffi_find_symbol(const char *function_name);
int scml_ffi_call_native(void *function_ptr, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret);
int scml_ffi_call_native_ex(void *function_ptr, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret);
int scml_ffi_call_native_by_name(const char *function_name, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret);
int scml_ffi_call_native_by_name_ex(const char *function_name, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret);
int scml_ffi_declare_function(const char *function_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count);
int scml_ffi_declare_function_text(const char *function_name, const char *return_type, const char *arg_types);
int scml_ffi_undeclare_function(const char *function_name);
const ScmlFFISignature *scml_ffi_get_declared_signature(const char *function_name);
const char *scml_ffi_last_error(void);
ScmlFFIType scml_ffi_parse_type(const char *name, ScmlFFIType fallback);
ScmlFFIReturnType scml_ffi_parse_return_type(const char *name, ScmlFFIReturnType fallback);
size_t scml_ffi_parse_arg_types(const char *spec, ScmlFFIType *out_types, size_t max_types);
void *scml_ffi_alloc(size_t size);
void *scml_ffi_alloc_cstring(const char *text);
char *scml_ffi_read_cstring(const void *ptr);
int scml_ffi_free_memory(void *ptr);
void *scml_ffi_ptr_add(void *ptr, intptr_t offset);
int scml_ffi_memory_read(void *base, size_t offset, ScmlFFIType type, ScmlValue *ret);
int scml_ffi_memory_write(void *base, size_t offset, ScmlFFIType type, const ScmlValue *value);
int scml_ffi_memory_copy(void *dst, const void *src, size_t size);
int scml_ffi_memory_set(void *dst, int value, size_t size);
size_t scml_ffi_memory_block_size(void *ptr);
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
