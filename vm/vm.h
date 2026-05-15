#ifndef SCML_VM_H
#define SCML_VM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SCML_STACK_MAX 1024
#define SCML_VARS_MAX 256
#define SCML_HEAP_MAX 8192
#define SCML_ENTITIES_MAX 128

typedef enum ScmlValueType { SCML_VAL_INT, SCML_VAL_STRING } ScmlValueType;
typedef struct ScmlValue { ScmlValueType type; int32_t integer; char *string; } ScmlValue;
typedef struct ScmlEntity { int active; int id; char model[64]; int x,y,z; } ScmlEntity;
typedef struct ScmlVM ScmlVM;

ScmlVM *scml_vm_create(void);
void scml_vm_destroy(ScmlVM *vm);
int scml_vm_load_file(ScmlVM *vm, const char *path, char *err, size_t err_size);
int scml_vm_run(ScmlVM *vm, char *err, size_t err_size);
void scml_vm_set_trace(ScmlVM *vm, int enabled);
int scml_vm_set_int(ScmlVM *vm, const char *name, int32_t value);
int scml_vm_get_int(ScmlVM *vm, const char *name, int32_t *out);
#ifdef __cplusplus
}
#endif
#endif
