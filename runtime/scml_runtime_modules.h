#ifndef SCML_RUNTIME_MODULES_H
#define SCML_RUNTIME_MODULES_H

#include "../vm/vm.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ScmlRuntimeModuleFunction)(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data);

typedef struct ScmlRuntimeFunctionEntry {
    const char *name;
    ScmlRuntimeModuleFunction fn;
} ScmlRuntimeFunctionEntry;

typedef struct ScmlRuntimeBackendVTable {
    const char *backend_name;
    const ScmlRuntimeFunctionEntry *functions;
    size_t function_count;
    void *user_data;
} ScmlRuntimeBackendVTable;

int scml_runtime_register_module(ScmlVM *vm,
                                 const char *module_name,
                                 const ScmlRuntimeFunctionEntry *functions,
                                 size_t function_count,
                                 void *user_data);
int scml_runtime_register_backend(ScmlVM *vm, const char *module_name, const ScmlRuntimeBackendVTable *backend);
int scml_runtime_select_backend(ScmlVM *vm, const char *module_name, const char *backend_name);
int scml_runtime_unregister_module(ScmlVM *vm, const char *module_name);

int scml_runtime_resolve_module(ScmlVM *vm, const char *module_name);
int scml_runtime_call_module_function(ScmlVM *vm, const char *qualified_name, const ScmlValue *args, size_t arg_count, ScmlValue *ret);
size_t scml_runtime_registered_module_count(void);
int scml_runtime_install_builtin_module_registry(ScmlVM *vm);

#ifdef __cplusplus
}
#endif

#endif
