#ifndef SCML_VM_H
#define SCML_VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCML_STACK_MAX 1024
#define SCML_VARS_MAX 256
#define SCML_LOCAL_VARS_MAX 64
#define SCML_HEAP_OBJECTS_MAX 512
#define SCML_CALL_STACK_MAX 128
#define SCML_EVENTS_MAX 64
#define SCML_EVENT_HANDLERS_MAX 16
#define SCML_EVENT_QUEUE_MAX 256
#define SCML_ENTITIES_MAX 128
#define SCML_NATIVE_FUNCS_MAX 128
#define SCML_CLASSES_MAX 128
#define SCML_OBJECTS_MAX 512
#define SCML_OBJECT_FIELDS_MAX 64
#define SCML_CLASS_METHODS_MAX 32

typedef enum ScmlValueType { SCML_VAL_INT, SCML_VAL_STRING, SCML_VAL_FLOAT, SCML_VAL_BOOL, SCML_VAL_NULL, SCML_VAL_OBJECT } ScmlValueType;

typedef struct ScmlValue {
    ScmlValueType type;
    int64_t integer;
    float real;
    uint8_t int_bits;
    char *string;
} ScmlValue;

typedef struct ScmlEntity {
    int active;
    int id;
    char model[64];
    float x;
    float y;
    float z;
    int health;
} ScmlEntity;

typedef struct ScmlVM ScmlVM;
typedef int (*ScmlNativeFunc)(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data);

ScmlVM *scml_vm_create(void);
void scml_vm_destroy(ScmlVM *vm);
int scml_vm_load_file(ScmlVM *vm, const char *path, char *err, size_t err_size);
int scml_vm_run(ScmlVM *vm, char *err, size_t err_size);
int scml_vm_update(ScmlVM *vm, char *err, size_t err_size);
int scml_vm_step(ScmlVM *vm, char *err, size_t err_size);
void scml_vm_set_trace(ScmlVM *vm, int enabled);
int scml_vm_set_int(ScmlVM *vm, const char *name, int32_t value);
int scml_vm_get_int(ScmlVM *vm, const char *name, int32_t *out);
int scml_vm_set_int64(ScmlVM *vm, const char *name, int64_t value);
int scml_vm_get_int64(ScmlVM *vm, const char *name, int64_t *out);
int scml_vm_set_float(ScmlVM *vm, const char *name, float value);
int scml_vm_get_float(ScmlVM *vm, const char *name, float *out);
int scml_vm_set_string(ScmlVM *vm, const char *name, const char *value);
int scml_vm_register_function(ScmlVM *vm, const char *name, ScmlNativeFunc fn, void *user_data);
int scml_vm_bind_event(ScmlVM *vm, const char *event_name, uint32_t handler_pc);
int scml_vm_trigger_event(ScmlVM *vm, const char *event_name, char *err, size_t err_size);
void scml_vm_dump_memory(ScmlVM *vm, FILE *out);
size_t scml_vm_pc(const ScmlVM *vm);
uint32_t scml_vm_current_line(const ScmlVM *vm);
int scml_vm_find_label(const ScmlVM *vm, const char *label, uint32_t *out_pc);
void scml_vm_clear_events(ScmlVM *vm);
ScmlValue scml_value_int(int32_t value);
ScmlValue scml_value_int64(int64_t value);
ScmlValue scml_value_float(float value);
ScmlValue scml_value_bool(int value);
ScmlValue scml_value_null(void);
ScmlValue scml_value_object(uint32_t object_id);
ScmlValue scml_value_string(const char *value);
void scml_value_dispose(ScmlValue *value);

#ifdef __cplusplus
}
#endif

#endif
