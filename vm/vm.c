#include "vm.h"

#include "../opcode/opcode.h"
#include "../ffi/scml_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct Var { char *name; ScmlValue value; } Var;
typedef struct HeapObject { int active; uint32_t id; size_t size; size_t elem_size; int pinned; unsigned char *data; } HeapObject;
typedef struct CallFrame { size_t return_pc; Var locals[SCML_LOCAL_VARS_MAX]; size_t local_count; } CallFrame;
typedef struct EventEntry { char *name; uint32_t handlers[SCML_EVENT_HANDLERS_MAX]; size_t handler_count; } EventEntry;
typedef struct EventQueueItem { char *name; size_t handler_index; uint32_t direct_pc; uint32_t task_id; } EventQueueItem;
typedef struct AsyncTask { int active; int done; uint32_t id; uint32_t entry_pc; } AsyncTask;
typedef struct LineEntry { uint32_t pc; uint32_t line; } LineEntry;
typedef struct LabelEntry { char *name; uint32_t pc; } LabelEntry;
typedef struct NativeFunc { char *name; ScmlNativeFunc fn; void *user_data; } NativeFunc;
typedef struct NativeModule { char *name; ScmlModuleResolver resolver; void *user_data; } NativeModule;

typedef struct DecOp { ScmlOperandType type; int32_t i; float f; char *s; } DecOp;

static void error_at(ScmlVM *vm, size_t pc, char *err, size_t err_size, const char *message);

struct ScmlVM {
    uint8_t *code;
    size_t code_size;
    size_t pc;
    int halted;
    int trace;
    ScmlValue stack[SCML_STACK_MAX];
    size_t sp;
    Var globals[SCML_VARS_MAX];
    size_t global_count;
    CallFrame calls[SCML_CALL_STACK_MAX];
    size_t call_depth;
    HeapObject heap[SCML_HEAP_OBJECTS_MAX];
    uint32_t next_ref;
    EventEntry events[SCML_EVENTS_MAX];
    size_t event_count;
    EventQueueItem event_queue[SCML_EVENT_QUEUE_MAX];
    size_t event_head;
    size_t event_tail;
    LineEntry *lines;
    size_t line_count;
    LabelEntry *labels;
    size_t label_count;
    NativeFunc natives[SCML_NATIVE_FUNCS_MAX];
    size_t native_count;
    NativeModule modules[SCML_NATIVE_MODULES_MAX];
    size_t module_count;
    AsyncTask async_tasks[SCML_ASYNC_TASKS_MAX];
    uint32_t next_task_id;
    uint32_t current_task_id;
};

static char *xstrdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s ? s : "", n + 1);
    return r;
}

static void value_free(ScmlValue *v) {
    if (v->type == SCML_VAL_STRING) free(v->string);
    v->type = SCML_VAL_INT;
    v->integer = 0;
    v->real = 0.0f;
    v->string = NULL;
    v->pointer = 0;
}

static ScmlValue value_int(int32_t x) {
    ScmlValue v;
    v.type = SCML_VAL_INT;
    v.integer = x;
    v.real = (float)x;
    v.string = NULL;
    v.pointer = (uintptr_t)(uint32_t)x;
    return v;
}

static ScmlValue value_str(const char *s) {
    ScmlValue v;
    v.type = SCML_VAL_STRING;
    v.integer = 0;
    v.real = 0.0f;
    v.string = xstrdup(s ? s : "");
    v.pointer = (uintptr_t)(v.string ? v.string : "");
    return v;
}

static ScmlValue value_float(float x) {
    ScmlValue v;
    v.type = SCML_VAL_FLOAT;
    v.integer = (int32_t)x;
    v.real = x;
    v.string = NULL;
    v.pointer = (uintptr_t)(intptr_t)x;
    return v;
}

static ScmlValue value_pointer(uintptr_t x) {
    ScmlValue v;
    v.type = SCML_VAL_POINTER;
    v.integer = (int32_t)x;
    v.real = (float)x;
    v.string = NULL;
    v.pointer = x;
    return v;
}

static ScmlValue value_clone(const ScmlValue *v) {
    if (v->type == SCML_VAL_STRING) return value_str(v->string);
    if (v->type == SCML_VAL_FLOAT) return value_float(v->real);
    if (v->type == SCML_VAL_POINTER) return value_pointer(v->pointer);
    return value_int(v->integer);
}

static int value_to_int(const ScmlValue *v) {
    if (v->type == SCML_VAL_INT) return v->integer;
    if (v->type == SCML_VAL_POINTER) return (int)(intptr_t)v->pointer;
    if (v->type == SCML_VAL_FLOAT) return (int)v->real;
    return atoi(v->string ? v->string : "0");
}

static float value_to_float(const ScmlValue *v) {
    if (v->type == SCML_VAL_FLOAT) return v->real;
    if (v->type == SCML_VAL_POINTER) return (float)v->pointer;
    if (v->type == SCML_VAL_INT) return (float)v->integer;
    return v->string ? (float)atof(v->string) : 0.0f;
}

static int values_equal(const ScmlValue *a, const ScmlValue *b) {
    if (a->type == SCML_VAL_STRING && b->type == SCML_VAL_STRING) {
        const char *as = a->string ? a->string : "";
        const char *bs = b->string ? b->string : "";
        return strcmp(as, bs) == 0;
    }
    if (a->type == SCML_VAL_FLOAT || b->type == SCML_VAL_FLOAT) {
        return value_to_float(a) == value_to_float(b);
    }
    return value_to_int(a) == value_to_int(b);
}

static int values_compare(const ScmlValue *a, const ScmlValue *b) {
    if (a->type == SCML_VAL_STRING && b->type == SCML_VAL_STRING) {
        const char *as = a->string ? a->string : "";
        const char *bs = b->string ? b->string : "";
        int cmp = strcmp(as, bs);
        return (cmp > 0) - (cmp < 0);
    }
    if (a->type == SCML_VAL_FLOAT || b->type == SCML_VAL_FLOAT) {
        float x = value_to_float(a);
        float y = value_to_float(b);
        return (x > y) - (x < y);
    }
    int x = value_to_int(a);
    int y = value_to_int(b);
    return (x > y) - (x < y);
}

static const char *value_to_cstr(const ScmlValue *v, char *buf, size_t n) {
    if (v->type == SCML_VAL_STRING) return v->string ? v->string : "";
    if (v->type == SCML_VAL_FLOAT) { snprintf(buf, n, "%g", v->real); return buf; }
    if (v->type == SCML_VAL_POINTER) { snprintf(buf, n, "0x%llx", (unsigned long long)v->pointer); return buf; }
    snprintf(buf, n, "%d", v->integer);
    return buf;
}

static uint8_t r8(ScmlVM *vm) { return vm->code[vm->pc++]; }
static uint16_t r16(ScmlVM *vm) { uint16_t v = vm->code[vm->pc] | (vm->code[vm->pc + 1] << 8); vm->pc += 2; return v; }
static uint32_t r32(ScmlVM *vm) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= ((uint32_t)vm->code[vm->pc++]) << (i * 8); return v; }
static float rf32(ScmlVM *vm) { union { uint32_t u; float f; } cvt; cvt.u = r32(vm); return cvt.f; }

static Var *find_var_in(Var *vars, size_t *count, size_t max, const char *name, int create) {
    for (size_t i = 0; i < *count; i++) if (strcmp(vars[i].name, name) == 0) return &vars[i];
    if (!create || *count >= max) return NULL;
    char *owned_name = xstrdup(name);
    if (!owned_name) return NULL;
    Var *v = &vars[(*count)++];
    v->name = owned_name;
    v->value = value_int(0);
    return v;
}

static int is_global_name(const char *name) { return name && name[0] == '$'; }

static Var *find_var(ScmlVM *vm, const char *name, int create) {
    if (!is_global_name(name) && vm->call_depth > 0) {
        CallFrame *frame = &vm->calls[vm->call_depth - 1];
        return find_var_in(frame->locals, &frame->local_count, SCML_LOCAL_VARS_MAX, name, create);
    }
    return find_var_in(vm->globals, &vm->global_count, SCML_VARS_MAX, name, create);
}

static int set_var(ScmlVM *vm, const char *name, ScmlValue value) {
    Var *var = find_var(vm, name, 1);
    if (!var) {
        value_free(&value);
        return 0;
    }
    value_free(&var->value);
    var->value = value;
    return 1;
}

static ScmlValue eval(ScmlVM *vm, DecOp *o) {
    if (o->type == SCML_OPERAND_INT || o->type == SCML_OPERAND_ADDRESS) return value_int(o->i);
    if (o->type == SCML_OPERAND_FLOAT) return value_float(o->f);
    if (o->type == SCML_OPERAND_STRING) return value_str(o->s);
    Var *v = find_var(vm, o->s, 0);
    return v ? value_clone(&v->value) : value_int(0);
}

static int checked_int_arg(ScmlVM *vm, DecOp *op, int min_value, int max_value, const char *message, int *out, size_t ins_pc, char *err, size_t err_size) {
    ScmlValue v = eval(vm, op);
    int value = value_to_int(&v);
    value_free(&v);
    if (value < min_value || value > max_value) {
        error_at(vm, ins_pc, err, err_size, message);
        return 0;
    }
    *out = value;
    return 1;
}

static void free_decoded(DecOp *ops, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (ops[i].type == SCML_OPERAND_STRING || ops[i].type == SCML_OPERAND_VAR) free(ops[i].s);
    }
}

static int decode_operand(ScmlVM *vm, DecOp *o, char *err, size_t err_size) {
    if (vm->pc >= vm->code_size) { snprintf(err, err_size, "truncated operand"); return 0; }
    o->type = (ScmlOperandType)r8(vm);
    if (o->type == SCML_OPERAND_INT || o->type == SCML_OPERAND_ADDRESS) {
        if (vm->pc + 4 > vm->code_size) { snprintf(err, err_size, "truncated integer operand"); return 0; }
        o->i = (int32_t)r32(vm);
        return 1;
    }
    if (o->type == SCML_OPERAND_FLOAT) {
        if (vm->pc + 4 > vm->code_size) { snprintf(err, err_size, "truncated float operand"); return 0; }
        o->f = rf32(vm);
        return 1;
    }
    if (o->type == SCML_OPERAND_STRING || o->type == SCML_OPERAND_VAR) {
        if (vm->pc + 2 > vm->code_size) { snprintf(err, err_size, "truncated string length"); return 0; }
        uint16_t n = r16(vm);
        if (vm->pc + n > vm->code_size) { snprintf(err, err_size, "truncated string operand"); return 0; }
        o->s = (char *)malloc(n + 1);
        if (!o->s) { snprintf(err, err_size, "out of memory"); return 0; }
        memcpy(o->s, vm->code + vm->pc, n);
        o->s[n] = '\0';
        vm->pc += n;
        return 1;
    }
    snprintf(err, err_size, "bad operand tag %d", o->type);
    return 0;
}

static void clear_frame(CallFrame *frame) {
    for (size_t i = 0; i < frame->local_count; i++) {
        free(frame->locals[i].name);
        value_free(&frame->locals[i].value);
    }
    frame->local_count = 0;
}

static uint32_t current_line(ScmlVM *vm, size_t pc) {
    uint32_t line = 0;
    for (size_t i = 0; i < vm->line_count; i++) {
        if (vm->lines[i].pc <= pc) line = vm->lines[i].line;
        else break;
    }
    return line;
}

static void error_at(ScmlVM *vm, size_t pc, char *err, size_t err_size, const char *message) {
    uint32_t line = current_line(vm, pc);
    if (line) snprintf(err, err_size, "line %u: %s", line, message);
    else snprintf(err, err_size, "%s", message);
}

static int valid_pc_target(ScmlVM *vm, size_t target) {
    return target <= vm->code_size;
}

static int require_var_operand(ScmlVM *vm, const DecOp *op, size_t pc, char *err, size_t err_size, const char *message) {
    if (op->type == SCML_OPERAND_VAR && op->s) return 1;
    error_at(vm, pc, err, err_size, message);
    return 0;
}

static int require_address_operand(ScmlVM *vm, const DecOp *op, size_t pc, char *err, size_t err_size, const char *message) {
    if (op->type == SCML_OPERAND_ADDRESS && valid_pc_target(vm, (size_t)op->i)) return 1;
    error_at(vm, pc, err, err_size, message);
    return 0;
}

static int require_native_name_operand(ScmlVM *vm, const DecOp *op, size_t pc, char *err, size_t err_size) {
    if ((op->type == SCML_OPERAND_STRING || op->type == SCML_OPERAND_VAR) && op->s) return 1;
    error_at(vm, pc, err, err_size, "native call requires string/function name");
    return 0;
}

static int require_string_operand(ScmlVM *vm, const DecOp *op, size_t pc, char *err, size_t err_size, const char *message) {
    if (op->type == SCML_OPERAND_STRING && op->s) return 1;
    error_at(vm, pc, err, err_size, message);
    return 0;
}

static int validate_decoded_operands(ScmlVM *vm, ScmlOpcode op, DecOp *ops, uint8_t argc, size_t pc, char *err, size_t err_size) {
    switch (op) {
    case SCML_OP_STORE:
        return require_var_operand(vm, &ops[0], pc, err, err_size, "SET destination must be a variable");
    case SCML_OP_ADD: case SCML_OP_SUB: case SCML_OP_MUL: case SCML_OP_DIV: case SCML_OP_MOD:
    case SCML_OP_SIN: case SCML_OP_COS: case SCML_OP_TAN: case SCML_OP_SQRT: case SCML_OP_ATAN2:
    case SCML_OP_FLOOR: case SCML_OP_CEIL: case SCML_OP_ROUND: case SCML_OP_ABS:
    case SCML_OP_STR_REPEAT: case SCML_OP_STRCAT: case SCML_OP_TO_INT: case SCML_OP_TO_FLOAT:
    case SCML_OP_BIT_AND: case SCML_OP_BIT_OR: case SCML_OP_BIT_XOR: case SCML_OP_BIT_NOT:
    case SCML_OP_SHL: case SCML_OP_SHR: case SCML_OP_POW: case SCML_OP_STRLEN: case SCML_OP_SUBSTR:
    case SCML_OP_ARRAY_LEN: case SCML_OP_SPAN_READ_U8:
        return require_var_operand(vm, &ops[0], pc, err, err_size, "opcode destination must be a variable");
    case SCML_OP_JMP:
        return require_address_operand(vm, &ops[0], pc, err, err_size, "JUMP target must be a valid label address");
    case SCML_OP_IF_EQ: case SCML_OP_IF_NE: case SCML_OP_IF_GT: case SCML_OP_IF_LT: case SCML_OP_IF_GE: case SCML_OP_IF_LE:
        return require_address_operand(vm, &ops[2], pc, err, err_size, "conditional target must be a valid label address");
    case SCML_OP_CALL:
        if (ops[0].type == SCML_OPERAND_ADDRESS) return require_address_operand(vm, &ops[0], pc, err, err_size, "CALL target must be a valid label address");
        return require_native_name_operand(vm, &ops[0], pc, err, err_size);
    case SCML_OP_EVENT_BIND:
        return require_address_operand(vm, &ops[1], pc, err, err_size, "event handler must be a valid label address");
    case SCML_OP_ENTITY_SPAWN:
        return require_var_operand(vm, &ops[4], pc, err, err_size, "ENTITY_SPAWN output must be a variable");
    case SCML_OP_HEAP_ALLOC: case SCML_OP_ARRAY_CREATE: case SCML_OP_SPAN_CREATE:
        return require_var_operand(vm, &ops[1], pc, err, err_size, "allocation output must be a variable");
    case SCML_OP_HEAP_LOAD:
        return require_var_operand(vm, &ops[0], pc, err, err_size, "heap load output must be a variable");
    case SCML_OP_CALL_NATIVE:
        return require_native_name_operand(vm, &ops[0], pc, err, err_size);
    case SCML_OP_ASYNC_SPAWN:
        return require_address_operand(vm, &ops[0], pc, err, err_size, "ASYNC_SPAWN target must be a valid label address") &&
               require_var_operand(vm, &ops[1], pc, err, err_size, "ASYNC_SPAWN output must be a variable");
    case SCML_OP_ASYNC_DONE:
        return require_var_operand(vm, &ops[1], pc, err, err_size, "ASYNC_DONE output must be a variable");
    case SCML_OP_TYPE_DECL:
        return require_var_operand(vm, &ops[0], pc, err, err_size, "TYPE_DECL target must be a variable") &&
               require_string_operand(vm, &ops[1], pc, err, err_size, "TYPE_DECL type must be a string");
    case SCML_OP_TYPE_ASSERT:
        return require_var_operand(vm, &ops[2], pc, err, err_size, "TYPE_ASSERT output must be a variable");
    case SCML_OP_FILE_READ:
        return require_var_operand(vm, &ops[1], pc, err, err_size, "FILE_READ output must be a variable");
    default:
        (void)argc;
        return 1;
    }
}

static HeapObject *heap_find(ScmlVM *vm, uint32_t ref) {
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) {
        if (vm->heap[i].active && vm->heap[i].id == ref) return &vm->heap[i];
    }
    return NULL;
}

static int heap_alloc_typed(ScmlVM *vm, size_t size, size_t elem_size, int pinned, uint32_t *out_ref) {
    if (elem_size == 0) return 0;
    if (size > (SIZE_MAX / elem_size)) return 0;
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) {
        if (!vm->heap[i].active) {
            size_t bytes = size * elem_size;
            vm->heap[i].data = (unsigned char *)calloc(bytes ? bytes : elem_size, 1);
            if (!vm->heap[i].data) return 0;
            vm->heap[i].active = 1;
            vm->heap[i].id = vm->next_ref++;
            vm->heap[i].size = size;
            vm->heap[i].elem_size = elem_size;
            vm->heap[i].pinned = pinned;
            *out_ref = vm->heap[i].id;
            return 1;
        }
    }
    return 0;
}

static int heap_alloc(ScmlVM *vm, size_t size, uint32_t *out_ref) {
    return heap_alloc_typed(vm, size, sizeof(int32_t), 0, out_ref);
}

static int heap_fill(HeapObject *obj, size_t start, size_t count, int32_t value) {
    if (!obj || start > obj->size || count > obj->size - start) return 0;
    if (obj->elem_size == 1) {
        memset(obj->data + start, (unsigned char)value, count);
        return 1;
    }
    if (obj->elem_size == sizeof(int32_t)) {
        int32_t *cells = (int32_t *)obj->data;
        for (size_t i = 0; i < count; i++) cells[start + i] = value;
        return 1;
    }
    return 0;
}

static int heap_store_i32(HeapObject *obj, size_t index, int32_t value) {
    if (!obj || index >= obj->size) return 0;
    if (obj->elem_size == 1) {
        obj->data[index] = (unsigned char)value;
        return 1;
    }
    if (obj->elem_size == sizeof(int32_t)) {
        ((int32_t *)obj->data)[index] = value;
        return 1;
    }
    return 0;
}

static int heap_load_i32(HeapObject *obj, size_t index, int32_t *out) {
    if (!obj || !out || index >= obj->size) return 0;
    if (obj->elem_size == 1) {
        *out = obj->data[index];
        return 1;
    }
    if (obj->elem_size == sizeof(int32_t)) {
        *out = ((int32_t *)obj->data)[index];
        return 1;
    }
    return 0;
}


static NativeFunc *native_find(ScmlVM *vm, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < vm->native_count; i++) {
        if (vm->natives[i].name && strcmp(vm->natives[i].name, name) == 0) return &vm->natives[i];
    }
    return NULL;
}


static NativeModule *module_find(ScmlVM *vm, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < vm->module_count; i++) {
        if (vm->modules[i].name && strcmp(vm->modules[i].name, name) == 0) return &vm->modules[i];
    }
    return NULL;
}

static EventEntry *event_find(ScmlVM *vm, const char *name, int create) {
    if (!name) return NULL;
    for (size_t i = 0; i < vm->event_count; i++) if (vm->events[i].name && strcmp(vm->events[i].name, name) == 0) return &vm->events[i];
    if (!create || vm->event_count >= SCML_EVENTS_MAX) return NULL;
    char *owned_name = xstrdup(name);
    if (!owned_name) return NULL;
    EventEntry *event = &vm->events[vm->event_count++];
    event->name = owned_name;
    event->handler_count = 0;
    return event;
}

int scml_vm_bind_event(ScmlVM *vm, const char *event_name, uint32_t handler_pc) {
    if (!vm || handler_pc > vm->code_size) return 0;
    EventEntry *event = event_find(vm, event_name, 1);
    if (!event || event->handler_count >= SCML_EVENT_HANDLERS_MAX) return 0;
    event->handlers[event->handler_count++] = handler_pc;
    return 1;
}

int scml_vm_trigger_event(ScmlVM *vm, const char *event_name, char *err, size_t err_size) {
    size_t next = (vm->event_tail + 1) % SCML_EVENT_QUEUE_MAX;
    if (next == vm->event_head) {
        if (err && err_size) snprintf(err, err_size, "event queue full");
        return 0;
    }
    vm->event_queue[vm->event_tail].name = xstrdup(event_name);
    if (!vm->event_queue[vm->event_tail].name) {
        if (err && err_size) snprintf(err, err_size, "out of memory");
        return 0;
    }
    vm->event_queue[vm->event_tail].handler_index = 0;
    vm->event_queue[vm->event_tail].direct_pc = UINT32_MAX;
    vm->event_queue[vm->event_tail].task_id = 0;
    vm->event_tail = next;
    return 1;
}

static int queue_event_handler(ScmlVM *vm, const char *event_name, size_t handler_index) {
    size_t next = (vm->event_tail + 1) % SCML_EVENT_QUEUE_MAX;
    if (next == vm->event_head) return 0;
    vm->event_queue[vm->event_tail].name = xstrdup(event_name);
    if (!vm->event_queue[vm->event_tail].name) return 0;
    vm->event_queue[vm->event_tail].handler_index = handler_index;
    vm->event_queue[vm->event_tail].direct_pc = UINT32_MAX;
    vm->event_queue[vm->event_tail].task_id = 0;
    vm->event_tail = next;
    return 1;
}

static AsyncTask *async_task_find(ScmlVM *vm, uint32_t id) {
    for (size_t i = 0; i < SCML_ASYNC_TASKS_MAX; i++) {
        if (vm->async_tasks[i].active && vm->async_tasks[i].id == id) return &vm->async_tasks[i];
    }
    return NULL;
}

static int queue_async_task(ScmlVM *vm, uint32_t pc, uint32_t task_id) {
    if (pc > vm->code_size) return 0;
    size_t next = (vm->event_tail + 1) % SCML_EVENT_QUEUE_MAX;
    if (next == vm->event_head) return 0;
    vm->event_queue[vm->event_tail].name = NULL;
    vm->event_queue[vm->event_tail].handler_index = 0;
    vm->event_queue[vm->event_tail].direct_pc = pc;
    vm->event_queue[vm->event_tail].task_id = task_id;
    vm->event_tail = next;
    return 1;
}

static int dispatch_next_event(ScmlVM *vm) {
    if (vm->event_head == vm->event_tail) return 0;
    EventQueueItem item = vm->event_queue[vm->event_head];
    vm->event_head = (vm->event_head + 1) % SCML_EVENT_QUEUE_MAX;
    if (item.direct_pc != UINT32_MAX) {
        vm->pc = item.direct_pc;
        vm->current_task_id = item.task_id;
        vm->halted = 0;
        return 1;
    }
    char *event_name = item.name;
    size_t handler_index = item.handler_index;
    EventEntry *event = event_find(vm, event_name, 0);
    if (event && handler_index < event->handler_count) {
        if (handler_index + 1 < event->handler_count) queue_event_handler(vm, event_name, handler_index + 1);
        vm->pc = event->handlers[handler_index];
        vm->current_task_id = 0;
        vm->halted = 0;
        free(event_name);
        return 1;
    }
    free(event_name);
    return 0;
}

ScmlVM *scml_vm_create(void) {
    ScmlVM *vm = (ScmlVM *)calloc(1, sizeof(*vm));
    if (!vm) return NULL;
    vm->next_ref = 1;
    vm->next_task_id = 1;
    return vm;
}

void scml_vm_destroy(ScmlVM *vm) {
    if (!vm) return;
    free(vm->code);
    free(vm->lines);
    for (size_t i = 0; i < vm->label_count; i++) free(vm->labels[i].name);
    free(vm->labels);
    for (size_t i = 0; i < vm->global_count; i++) { free(vm->globals[i].name); value_free(&vm->globals[i].value); }
    for (size_t i = 0; i < vm->call_depth; i++) clear_frame(&vm->calls[i]);
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) free(vm->heap[i].data);
    for (size_t i = 0; i < vm->event_count; i++) free(vm->events[i].name);
    for (size_t i = 0; i < vm->native_count; i++) free(vm->natives[i].name);
    for (size_t i = 0; i < vm->module_count; i++) free(vm->modules[i].name);
    while (vm->event_head != vm->event_tail) {
        free(vm->event_queue[vm->event_head].name);
        vm->event_head = (vm->event_head + 1) % SCML_EVENT_QUEUE_MAX;
    }
    for (size_t i = 0; i < vm->sp; i++) value_free(&vm->stack[i]);
    free(vm);
}

void scml_vm_set_trace(ScmlVM *vm, int enabled) { vm->trace = enabled; }

int scml_vm_load_file(ScmlVM *vm, const char *path, char *err, size_t err_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, err_size, "cannot open %s", path); return 0; }
    uint8_t hdr[18];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { snprintf(err, err_size, "bad header"); fclose(f); return 0; }
    uint32_t magic = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (hdr[3] << 24);
    uint16_t version = hdr[4] | (hdr[5] << 8);
    uint32_t code_size = hdr[6] | (hdr[7] << 8) | (hdr[8] << 16) | (hdr[9] << 24);
    uint32_t line_count = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);
    uint32_t label_count = hdr[14] | (hdr[15] << 8) | (hdr[16] << 16) | (hdr[17] << 24);
    if (magic != SCML_MAGIC || version != SCML_VERSION) { snprintf(err, err_size, "not a supported scmlbin"); fclose(f); return 0; }

    uint8_t *code = (uint8_t *)malloc(code_size ? code_size : 1);
    LineEntry *lines = (LineEntry *)calloc(line_count ? line_count : 1, sizeof(*lines));
    LabelEntry *labels = (LabelEntry *)calloc(label_count ? label_count : 1, sizeof(*labels));
    if (!code || !lines || !labels) { free(code); free(lines); free(labels); snprintf(err, err_size, "out of memory"); fclose(f); return 0; }
    if (fread(code, 1, code_size, f) != code_size) { free(code); free(lines); free(labels); snprintf(err, err_size, "truncated bytecode"); fclose(f); return 0; }
    for (uint32_t i = 0; i < line_count; i++) {
        uint8_t b[8];
        if (fread(b, 1, sizeof(b), f) != sizeof(b)) { free(code); free(lines); free(labels); snprintf(err, err_size, "truncated line table"); fclose(f); return 0; }
        lines[i].pc = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
        lines[i].line = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24);
    }
    for (uint32_t i = 0; i < label_count; i++) {
        uint8_t b[6];
        if (fread(b, 1, sizeof(b), f) != sizeof(b)) { free(code); free(lines); for (uint32_t j = 0; j < i; j++) free(labels[j].name); free(labels); snprintf(err, err_size, "truncated label table"); fclose(f); return 0; }
        labels[i].pc = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
        uint16_t n = b[4] | (b[5] << 8);
        labels[i].name = (char *)malloc((size_t)n + 1);
        if (!labels[i].name) { free(code); free(lines); for (uint32_t j = 0; j < i; j++) free(labels[j].name); free(labels); snprintf(err, err_size, "out of memory"); fclose(f); return 0; }
        if (fread(labels[i].name, 1, n, f) != n) { free(code); free(lines); for (uint32_t j = 0; j <= i; j++) free(labels[j].name); free(labels); snprintf(err, err_size, "truncated label name"); fclose(f); return 0; }
        labels[i].name[n] = '\0';
    }
    fclose(f);

    free(vm->code);
    free(vm->lines);
    for (size_t i = 0; i < vm->label_count; i++) free(vm->labels[i].name);
    free(vm->labels);
    vm->code = code;
    vm->code_size = code_size;
    vm->lines = lines;
    vm->line_count = line_count;
    vm->labels = labels;
    vm->label_count = label_count;
    vm->pc = 0;
    vm->halted = 0;
    return 1;
}

int scml_vm_set_int(ScmlVM *vm, const char *name, int32_t value) { return set_var(vm, name, value_int(value)); }

int scml_vm_get_int(ScmlVM *vm, const char *name, int32_t *out) {
    Var *v = find_var(vm, name, 0);
    if (!v) return 0;
    *out = value_to_int(&v->value);
    return 1;
}

ScmlValue scml_value_int(int32_t value) { return value_int(value); }
ScmlValue scml_value_float(float value) { return value_float(value); }
ScmlValue scml_value_string(const char *value) { return value_str(value); }
ScmlValue scml_value_pointer(uintptr_t value) { return value_pointer(value); }
void scml_value_dispose(ScmlValue *value) { value_free(value); }

int scml_vm_set_float(ScmlVM *vm, const char *name, float value) { return set_var(vm, name, value_float(value)); }

int scml_vm_get_float(ScmlVM *vm, const char *name, float *out) {
    Var *v = find_var(vm, name, 0);
    if (!v) return 0;
    *out = value_to_float(&v->value);
    return 1;
}

int scml_vm_set_string(ScmlVM *vm, const char *name, const char *value) { return set_var(vm, name, value_str(value)); }

int scml_vm_register_function(ScmlVM *vm, const char *name, ScmlNativeFunc fn, void *user_data) {
    if (!vm || !name || !fn) return 0;
    NativeFunc *existing = native_find(vm, name);
    if (existing) { existing->fn = fn; existing->user_data = user_data; return 1; }
    if (vm->native_count >= SCML_NATIVE_FUNCS_MAX) return 0;
    char *owned_name = xstrdup(name);
    if (!owned_name) return 0;
    NativeFunc *native = &vm->natives[vm->native_count++];
    native->name = owned_name;
    native->fn = fn;
    native->user_data = user_data;
    return 1;
}



int scml_vm_register_module(ScmlVM *vm, const char *name, ScmlModuleResolver resolver, void *user_data) {
    if (!vm || !name || !resolver) return 0;
    NativeModule *existing = module_find(vm, name);
    if (existing) { existing->resolver = resolver; existing->user_data = user_data; return 1; }
    if (vm->module_count >= SCML_NATIVE_MODULES_MAX) return 0;
    char *owned_name = xstrdup(name);
    if (!owned_name) return 0;
    NativeModule *module = &vm->modules[vm->module_count++];
    module->name = owned_name;
    module->resolver = resolver;
    module->user_data = user_data;
    return 1;
}

int scml_vm_unregister_module(ScmlVM *vm, const char *name) {
    if (!vm || !name) return 0;
    for (size_t i = 0; i < vm->module_count; i++) {
        if (strcmp(vm->modules[i].name, name) == 0) {
            free(vm->modules[i].name);
            if (i + 1 < vm->module_count) memmove(&vm->modules[i], &vm->modules[i + 1], (vm->module_count - i - 1) * sizeof(vm->modules[0]));
            vm->module_count--;
            return 1;
        }
    }
    return 0;
}

static ScmlFFIType vm_ffi_return_type(ScmlVM *vm) {
    Var *v = find_var(vm, "$FFI_RETURN_TYPE", 0);
    if (!v || v->value.type != SCML_VAL_STRING) return SCML_FFI_TYPE_INT32;
    return scml_ffi_parse_return_type(v->value.string, SCML_FFI_TYPE_INT32);
}

static ScmlFFISignature vm_ffi_signature(ScmlVM *vm, size_t arg_count, ScmlFFIType *arg_types, size_t max_arg_types) {
    ScmlFFISignature signature;
    signature.return_type = vm_ffi_return_type(vm);
    signature.arg_types = NULL;
    signature.arg_count = arg_count;
    Var *v = find_var(vm, "$FFI_ARG_TYPES", 0);
    if (v && v->value.type == SCML_VAL_STRING && arg_count <= max_arg_types) {
        size_t parsed = scml_ffi_parse_arg_types(v->value.string, arg_types, max_arg_types);
        if (parsed == arg_count) signature.arg_types = arg_types;
    }
    return signature;
}

static uintptr_t vm_value_to_pointer(const ScmlValue *value) {
    if (value->type == SCML_VAL_POINTER) return value->pointer;
    if (value->type == SCML_VAL_STRING) return (uintptr_t)(value->string ? value->string : "");
    return (uintptr_t)(intptr_t)value_to_int(value);
}

static int vm_ffi_type_arg(const ScmlValue *value, ScmlFFIType *out) {
    if (!value || !out || value->type != SCML_VAL_STRING) return 0;
    *out = scml_ffi_parse_type(value->string, SCML_FFI_TYPE_INT32);
    return 1;
}

static int vm_ffi_builtin(const char *name, const ScmlValue *args, size_t arg_count, ScmlValue *ret) {
    if (strcmp(name, "load_library") == 0 || strcmp(name, "ffi.load_library") == 0 || strcmp(name, "ffi.load") == 0) {
        if (arg_count < 1 || arg_count > 2 || args[0].type != SCML_VAL_STRING) return 0;
        unsigned int flags = arg_count == 2 ? (unsigned int)value_to_int(&args[1]) : (SCML_FFI_LOAD_NOW | SCML_FFI_LOAD_LOCAL);
        void *handle = scml_ffi_load_library_ex(args[0].string, flags);
        if (!handle) return 0;
        if (ret) *ret = value_pointer((uintptr_t)handle);
        return 1;
    }
    if (strcmp(name, "ffi.add_search_path") == 0) {
        if (arg_count != 1 || args[0].type != SCML_VAL_STRING) return 0;
        int ok = scml_ffi_add_search_path(args[0].string);
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.declare") == 0) {
        if (arg_count != 3 || args[0].type != SCML_VAL_STRING || args[1].type != SCML_VAL_STRING || args[2].type != SCML_VAL_STRING) return 0;
        int ok = scml_ffi_declare_function_text(args[0].string, args[1].string, args[2].string);
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.undeclare") == 0) {
        if (arg_count != 1 || args[0].type != SCML_VAL_STRING) return 0;
        int ok = scml_ffi_undeclare_function(args[0].string);
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "unload_library") == 0 || strcmp(name, "ffi.unload_library") == 0) {
        if (arg_count != 1) return 0;
        uintptr_t handle = args[0].type == SCML_VAL_POINTER ? args[0].pointer : (uintptr_t)(intptr_t)value_to_int(&args[0]);
        int ok = scml_ffi_unload_library((void *)handle);
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "get_symbol") == 0 || strcmp(name, "ffi.get_symbol") == 0) {
        if (arg_count != 2 || args[1].type != SCML_VAL_STRING) return 0;
        uintptr_t handle = args[0].type == SCML_VAL_POINTER ? args[0].pointer : (uintptr_t)(intptr_t)value_to_int(&args[0]);
        void *symbol = scml_ffi_get_symbol((void *)handle, args[1].string);
        if (!symbol) return 0;
        if (ret) *ret = value_pointer((uintptr_t)symbol);
        return 1;
    }
    if (strcmp(name, "ffi.alloc") == 0) {
        if (arg_count != 1) return 0;
        void *ptr = scml_ffi_alloc((size_t)value_to_int(&args[0]));
        if (!ptr) return 0;
        if (ret) *ret = value_pointer((uintptr_t)ptr);
        return 1;
    }
    if (strcmp(name, "ffi.cstring") == 0) {
        if (arg_count != 1 || args[0].type != SCML_VAL_STRING) return 0;
        void *ptr = scml_ffi_alloc_cstring(args[0].string);
        if (!ptr) return 0;
        if (ret) *ret = value_pointer((uintptr_t)ptr);
        return 1;
    }
    if (strcmp(name, "ffi.free") == 0) {
        if (arg_count != 1) return 0;
        int ok = scml_ffi_free_memory((void *)vm_value_to_pointer(&args[0]));
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.ptr_add") == 0) {
        if (arg_count != 2) return 0;
        void *ptr = scml_ffi_ptr_add((void *)vm_value_to_pointer(&args[0]), (intptr_t)value_to_int(&args[1]));
        if (!ptr) return 0;
        if (ret) *ret = value_pointer((uintptr_t)ptr);
        return 1;
    }
    if (strcmp(name, "ffi.read") == 0) {
        if (arg_count != 3) return 0;
        ScmlFFIType type;
        if (!vm_ffi_type_arg(&args[2], &type)) return 0;
        return scml_ffi_memory_read((void *)vm_value_to_pointer(&args[0]), (size_t)value_to_int(&args[1]), type, ret);
    }
    if (strcmp(name, "ffi.write") == 0) {
        if (arg_count != 4) return 0;
        ScmlFFIType type;
        if (!vm_ffi_type_arg(&args[2], &type)) return 0;
        int ok = scml_ffi_memory_write((void *)vm_value_to_pointer(&args[0]), (size_t)value_to_int(&args[1]), type, &args[3]);
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.memset") == 0) {
        if (arg_count != 3) return 0;
        int ok = scml_ffi_memory_set((void *)vm_value_to_pointer(&args[0]), value_to_int(&args[1]), (size_t)value_to_int(&args[2]));
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.memcpy") == 0) {
        if (arg_count != 3) return 0;
        int ok = scml_ffi_memory_copy((void *)vm_value_to_pointer(&args[0]), (const void *)vm_value_to_pointer(&args[1]), (size_t)value_to_int(&args[2]));
        if (ret) *ret = value_int(ok);
        return ok;
    }
    if (strcmp(name, "ffi.read_cstring") == 0) {
        if (arg_count != 1) return 0;
        char *text = scml_ffi_read_cstring((const void *)vm_value_to_pointer(&args[0]));
        if (!text) return 0;
        if (ret) *ret = value_str(text);
        free(text);
        return 1;
    }
    if (strcmp(name, "ffi.sizeof_block") == 0) {
        if (arg_count != 1) return 0;
        if (ret) *ret = value_int((int32_t)scml_ffi_memory_block_size((void *)vm_value_to_pointer(&args[0])));
        return 1;
    }
    if (strcmp(name, "ffi.stats") == 0) {
        ScmlFFIStats stats;
        scml_ffi_get_stats(&stats);
        if (ret) *ret = value_int((int32_t)stats.symbol_count);
        return 1;
    }
    return 0;
}

int scml_vm_call_native(ScmlVM *vm, const char *qualified_name, const ScmlValue *args, size_t arg_count, ScmlValue *ret) {
    if (!vm || !qualified_name || !qualified_name[0]) return 0;
    const char *dot = strchr(qualified_name, '.');
    if (dot && dot != qualified_name && dot[1] != '\0') {
        size_t mn = (size_t)(dot - qualified_name);
        char module_name[64];
        if (mn < sizeof(module_name)) {
            memcpy(module_name, qualified_name, mn);
            module_name[mn] = '\0';
            NativeModule *module = module_find(vm, module_name);
            if (module && module->resolver && module->resolver(vm, dot + 1, args, arg_count, ret, module->user_data)) return 1;
        }
    }
    if (vm_ffi_builtin(qualified_name, args, arg_count, ret)) return 1;
    ScmlFFIType arg_types[SCML_FFI_FALLBACK_MAX_ARGS];
    ScmlFFISignature signature = vm_ffi_signature(vm, arg_count, arg_types, sizeof(arg_types) / sizeof(arg_types[0]));
    return scml_ffi_call_native_by_name_ex(qualified_name, args, &signature, ret);
}

void *scml_vm_load_library(ScmlVM *vm, const char *path) {
    (void)vm;
    return scml_ffi_load_library(path);
}

int scml_vm_unload_library(ScmlVM *vm, void *handle) {
    (void)vm;
    return scml_ffi_unload_library(handle);
}

void *scml_vm_get_symbol(ScmlVM *vm, void *library_handle, const char *function_name) {
    (void)vm;
    return scml_ffi_get_symbol(library_handle, function_name);
}

size_t scml_vm_pc(const ScmlVM *vm) { return vm ? vm->pc : 0; }

uint32_t scml_vm_current_line(const ScmlVM *vm) {
    return vm ? current_line((ScmlVM *)vm, vm->pc) : 0;
}

int scml_vm_find_label(const ScmlVM *vm, const char *label, uint32_t *out_pc) {
    if (!vm || !label || !out_pc) return 0;
    const char *name = label[0] == '@' || label[0] == ':' ? label + 1 : label;
    for (size_t i = 0; i < vm->label_count; i++) {
        if (strcmp(vm->labels[i].name, name) == 0) {
            *out_pc = vm->labels[i].pc;
            return 1;
        }
    }
    return 0;
}

void scml_vm_clear_events(ScmlVM *vm) {
    if (!vm) return;
    for (size_t i = 0; i < vm->event_count; i++) free(vm->events[i].name);
    vm->event_count = 0;
    while (vm->event_head != vm->event_tail) {
        free(vm->event_queue[vm->event_head].name);
        vm->event_head = (vm->event_head + 1) % SCML_EVENT_QUEUE_MAX;
    }
    vm->event_head = vm->event_tail = 0;
}

void scml_vm_dump_memory(ScmlVM *vm, FILE *out) {
    fprintf(out, "SCML memory: globals=%zu heap_objects=", vm->global_count);
    size_t objects = 0;
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) if (vm->heap[i].active) objects++;
    fprintf(out, "%zu events=%zu queued=%zu\n", objects, vm->event_count, (vm->event_tail + SCML_EVENT_QUEUE_MAX - vm->event_head) % SCML_EVENT_QUEUE_MAX);
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) if (vm->heap[i].active) fprintf(out, "  ref=%u size=%zu elem_size=%zu pinned=%d\n", vm->heap[i].id, vm->heap[i].size, vm->heap[i].elem_size, vm->heap[i].pinned);
}

int scml_vm_step(ScmlVM *vm, char *err, size_t err_size) {
    if (vm->halted || vm->pc >= vm->code_size) {
        if (vm->current_task_id) {
            AsyncTask *task = async_task_find(vm, vm->current_task_id);
            if (task) task->done = 1;
            vm->current_task_id = 0;
        }
        if (dispatch_next_event(vm)) return 1;
        vm->halted = 1;
        return 0;
    }

    size_t ins_pc = vm->pc;
    if (vm->pc + 2 > vm->code_size) {
        error_at(vm, ins_pc, err, err_size, "truncated instruction header");
        vm->halted = 1;
        return -1;
    }
    ScmlOpcode op = (ScmlOpcode)r8(vm);
    uint8_t argc = r8(vm);
    const ScmlOpcodeInfo *op_info = scml_opcode_info(op);
    if (!op_info) { error_at(vm, ins_pc, err, err_size, "unknown opcode"); return -1; }
    if (argc < op_info->min_args || argc > op_info->max_args) {
        error_at(vm, ins_pc, err, err_size, "opcode operand count mismatch");
        return -1;
    }
    DecOp ops[8] = {0};
    if (argc > 8) { error_at(vm, ins_pc, err, err_size, "too many operands"); return -1; }
    for (uint8_t i = 0; i < argc; i++) {
        if (!decode_operand(vm, &ops[i], err, err_size)) { free_decoded(ops, i); return -1; }
    }
    if (!validate_decoded_operands(vm, op, ops, argc, ins_pc, err, err_size)) {
        free_decoded(ops, argc);
        return -1;
    }
    if (vm->trace) fprintf(stderr, "[SCML] pc=%zu line=%u op=%s argc=%u\n", ins_pc, current_line(vm, ins_pc), scml_opcode_name(op), argc);

    switch (op) {
    case SCML_OP_NOP: break;
    case SCML_OP_HALT: vm->halted = 1; break;
    case SCML_OP_END_THREAD:
        if (vm->current_task_id) {
            AsyncTask *task = async_task_find(vm, vm->current_task_id);
            if (task) task->done = 1;
            vm->current_task_id = 0;
        }
        vm->halted = !dispatch_next_event(vm);
        break;
    case SCML_OP_RETURN:
        if (vm->call_depth == 0) { error_at(vm, ins_pc, err, err_size, "return without call"); free_decoded(ops, argc); return -1; }
        clear_frame(&vm->calls[vm->call_depth - 1]);
        vm->pc = vm->calls[--vm->call_depth].return_pc;
        break;
    case SCML_OP_CALL:
        if (ops[0].type == SCML_OPERAND_ADDRESS) {
            if (vm->call_depth >= SCML_CALL_STACK_MAX) { error_at(vm, ins_pc, err, err_size, "call stack overflow"); free_decoded(ops, argc); return -1; }
            vm->calls[vm->call_depth].return_pc = vm->pc;
            vm->calls[vm->call_depth].local_count = 0;
            vm->call_depth++;
            vm->pc = (size_t)ops[0].i;
        } else {
            const char *name = ops[0].type == SCML_OPERAND_STRING ? ops[0].s : ops[0].s;
            NativeFunc *native = native_find(vm, name);
            ScmlValue args[7];
            memset(args, 0, sizeof(args));
            for (uint8_t ai = 1; ai < argc; ai++) args[ai - 1] = eval(vm, &ops[ai]);
            ScmlValue ret = value_int(0);
            int ok = (native && native->fn) ? native->fn(vm, args, argc - 1, &ret, native->user_data) : scml_vm_call_native(vm, name, args, argc - 1, &ret);
            for (uint8_t ai = 1; ai < argc; ai++) value_free(&args[ai - 1]);
            if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, scml_ffi_last_error()); free_decoded(ops, argc); return -1; }
            set_var(vm, "$RETVAL", value_clone(&ret));
            value_free(&ret);
        }
        break;
    case SCML_OP_PUSH_INT:
        if (vm->sp >= SCML_STACK_MAX) { error_at(vm, ins_pc, err, err_size, "stack overflow"); free_decoded(ops, argc); return -1; }
        vm->stack[vm->sp++] = value_int(ops[0].i);
        break;
    case SCML_OP_PUSH_STR:
        if (vm->sp >= SCML_STACK_MAX) { error_at(vm, ins_pc, err, err_size, "stack overflow"); free_decoded(ops, argc); return -1; }
        vm->stack[vm->sp++] = value_str(ops[0].s);
        break;
    case SCML_OP_LOAD: {
        if (vm->sp >= SCML_STACK_MAX) { error_at(vm, ins_pc, err, err_size, "stack overflow"); free_decoded(ops, argc); return -1; }
        vm->stack[vm->sp++] = eval(vm, &ops[0]);
        break;
    }
    case SCML_OP_STORE: {
        ScmlValue v = eval(vm, &ops[1]);
        if (!set_var(vm, ops[0].s, v)) { error_at(vm, ins_pc, err, err_size, "variable table full"); free_decoded(ops, argc); return -1; }
        break;
    }
    case SCML_OP_ADD: case SCML_OP_SUB: case SCML_OP_MUL: case SCML_OP_DIV: case SCML_OP_MOD: {
        ScmlValue a = eval(vm, &ops[1]);
        ScmlValue b = eval(vm, &ops[2]);
        int use_float = (a.type == SCML_VAL_FLOAT || b.type == SCML_VAL_FLOAT) && op != SCML_OP_MOD;
        if (use_float) {
            float x = value_to_float(&a), y = value_to_float(&b), r = 0.0f;
            if (op == SCML_OP_ADD) r = x + y;
            else if (op == SCML_OP_SUB) r = x - y;
            else if (op == SCML_OP_MUL) r = x * y;
            else if (op == SCML_OP_DIV) { if (y == 0.0f) { value_free(&a); value_free(&b); error_at(vm, ins_pc, err, err_size, "division by zero"); free_decoded(ops, argc); return -1; } r = x / y; }
            set_var(vm, ops[0].s, value_float(r));
        } else {
            int x = value_to_int(&a), y = value_to_int(&b), r = 0;
            if (op == SCML_OP_ADD) r = x + y;
            else if (op == SCML_OP_SUB) r = x - y;
            else if (op == SCML_OP_MUL) r = x * y;
            else if (op == SCML_OP_DIV) { if (y == 0) { value_free(&a); value_free(&b); error_at(vm, ins_pc, err, err_size, "division by zero"); free_decoded(ops, argc); return -1; } r = x / y; }
            else { if (y == 0) { value_free(&a); value_free(&b); error_at(vm, ins_pc, err, err_size, "mod by zero"); free_decoded(ops, argc); return -1; } r = x % y; }
            set_var(vm, ops[0].s, value_int(r));
        }
        value_free(&a);
        value_free(&b);
        break;
    }
    case SCML_OP_JMP:
        vm->pc = (size_t)ops[0].i;
        break;
    case SCML_OP_IF_EQ: case SCML_OP_IF_NE: case SCML_OP_IF_GT: case SCML_OP_IF_LT: case SCML_OP_IF_GE: case SCML_OP_IF_LE: {
        ScmlValue a = eval(vm, &ops[0]);
        ScmlValue b = eval(vm, &ops[1]);
        int cmp = values_compare(&a, &b);
        int eq = values_equal(&a, &b);
        int take = 0;
        if (op == SCML_OP_IF_EQ) take = eq;
        else if (op == SCML_OP_IF_NE) take = !eq;
        else if (op == SCML_OP_IF_GT) take = cmp > 0;
        else if (op == SCML_OP_IF_LT) take = cmp < 0;
        else if (op == SCML_OP_IF_GE) take = cmp >= 0;
        else take = cmp <= 0;
        value_free(&a);
        value_free(&b);
        if (take) vm->pc = (size_t)ops[2].i;
        break;
    }
    case SCML_OP_PRINT: case SCML_OP_LOG: case SCML_OP_PRINT_RAW: {
        ScmlValue v = eval(vm, &ops[0]);
        char b[256];
        fputs(value_to_cstr(&v, b, sizeof(b)), stdout);
        if (op != SCML_OP_PRINT_RAW) {
            fputc('\n', stdout);
            fflush(stdout);
        }
        value_free(&v);
        break;
    }
    case SCML_OP_CONSOLE_CLEAR:
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
        break;
    case SCML_OP_CONSOLE_COLOR: {
        int fg = 0, bg = -1;
        if (!checked_int_arg(vm, &ops[0], 0, 255, "CONSOLE_COLOR foreground must be 0..255", &fg, ins_pc, err, err_size)) { free_decoded(ops, argc); return -1; }
        if (argc > 1 && !checked_int_arg(vm, &ops[1], 0, 255, "CONSOLE_COLOR background must be 0..255", &bg, ins_pc, err, err_size)) { free_decoded(ops, argc); return -1; }
        if (bg >= 0) fprintf(stdout, "\033[38;5;%dm\033[48;5;%dm", fg, bg);
        else fprintf(stdout, "\033[38;5;%dm", fg);
        fflush(stdout);
        break;
    }
    case SCML_OP_CONSOLE_RESET:
        fputs("\033[0m", stdout);
        fflush(stdout);
        break;
    case SCML_OP_CONSOLE_MOVE: {
        int row = 0, col = 0;
        if (!checked_int_arg(vm, &ops[0], 1, 9999, "CONSOLE_MOVE row must be 1..9999", &row, ins_pc, err, err_size) ||
            !checked_int_arg(vm, &ops[1], 1, 9999, "CONSOLE_MOVE column must be 1..9999", &col, ins_pc, err, err_size)) { free_decoded(ops, argc); return -1; }
        fprintf(stdout, "\033[%d;%dH", row, col);
        fflush(stdout);
        break;
    }
    case SCML_OP_CONSOLE_ERASE_LINE:
        fputs("\033[2K", stdout);
        fflush(stdout);
        break;
    case SCML_OP_CONSOLE_STYLE: {
        int style = 0;
        if (!checked_int_arg(vm, &ops[0], 0, 9, "CONSOLE_STYLE must be 0..9", &style, ins_pc, err, err_size)) { free_decoded(ops, argc); return -1; }
        fprintf(stdout, "\033[%dm", style);
        fflush(stdout);
        break;
    }
    case SCML_OP_SIN: case SCML_OP_COS: case SCML_OP_TAN: case SCML_OP_SQRT:
    case SCML_OP_FLOOR: case SCML_OP_CEIL: case SCML_OP_ROUND: case SCML_OP_ABS: {
        ScmlValue v = eval(vm, &ops[1]);
        float x = value_to_float(&v), r = 0.0f;
        if (op == SCML_OP_SIN) r = sinf(x);
        else if (op == SCML_OP_COS) r = cosf(x);
        else if (op == SCML_OP_TAN) r = tanf(x);
        else if (op == SCML_OP_SQRT) { if (x < 0.0f) { value_free(&v); error_at(vm, ins_pc, err, err_size, "sqrt domain error"); free_decoded(ops, argc); return -1; } r = sqrtf(x); }
        else if (op == SCML_OP_FLOOR) r = floorf(x);
        else if (op == SCML_OP_CEIL) r = ceilf(x);
        else if (op == SCML_OP_ROUND) r = roundf(x);
        else r = fabsf(x);
        set_var(vm, ops[0].s, value_float(r));
        value_free(&v);
        break;
    }
    case SCML_OP_ATAN2: {
        ScmlValue yv = eval(vm, &ops[1]), xv = eval(vm, &ops[2]);
        set_var(vm, ops[0].s, value_float(atan2f(value_to_float(&yv), value_to_float(&xv))));
        value_free(&yv); value_free(&xv);
        break;
    }
    case SCML_OP_STR_REPEAT: {
        ScmlValue sv = eval(vm, &ops[1]), cv = eval(vm, &ops[2]);
        char sb[256];
        const char *str = value_to_cstr(&sv, sb, sizeof(sb));
        int count = value_to_int(&cv);
        if (count < 0) count = 0;
        size_t unit = strlen(str);
        if ((size_t)count > 0 && unit > (SIZE_MAX - 1) / (size_t)count) { value_free(&sv); value_free(&cv); error_at(vm, ins_pc, err, err_size, "string repeat too large"); free_decoded(ops, argc); return -1; }
        size_t total = unit * (size_t)count;
        char *tmp = (char *)malloc(total + 1);
        if (!tmp) { value_free(&sv); value_free(&cv); error_at(vm, ins_pc, err, err_size, "out of memory"); free_decoded(ops, argc); return -1; }
        char *w = tmp;
        for (int i = 0; i < count; i++) { memcpy(w, str, unit); w += unit; }
        tmp[total] = '\0';
        set_var(vm, ops[0].s, value_str(tmp));
        free(tmp);
        value_free(&sv); value_free(&cv);
        break;
    }
    case SCML_OP_WAIT: {
        fflush(stdout);
        ScmlValue v = eval(vm, &ops[0]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "runtime.wait", &v, 1, &ret);
        value_free(&v);
        value_free(&ret);
        if (!ok) { error_at(vm, ins_pc, err, err_size, "WAIT requires runtime.wait"); free_decoded(ops, argc); return -1; }
        if (vm->event_head != vm->event_tail) {
            size_t resume_pc = vm->pc;
            uint32_t resume_task = vm->current_task_id;
            if (!queue_async_task(vm, (uint32_t)resume_pc, resume_task)) { error_at(vm, ins_pc, err, err_size, "async queue full"); free_decoded(ops, argc); return -1; }
            if (!dispatch_next_event(vm)) { vm->pc = resume_pc; vm->current_task_id = resume_task; }
        }
        break;
    }
    case SCML_OP_FILE_READ: {
        ScmlValue path = eval(vm, &ops[0]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "file.read", &path, 1, &ret);
        value_free(&path);
        if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, "FILE_READ requires file.read"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[1].s, value_clone(&ret));
        value_free(&ret);
        break;
    }
    case SCML_OP_FILE_WRITE: {
        ScmlValue args[2];
        args[0] = eval(vm, &ops[0]);
        args[1] = eval(vm, &ops[1]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "file.write", args, 2, &ret);
        value_free(&args[0]); value_free(&args[1]); value_free(&ret);
        if (!ok) { error_at(vm, ins_pc, err, err_size, "FILE_WRITE requires file.write"); free_decoded(ops, argc); return -1; }
        break;
    }
    case SCML_OP_EVENT_BIND: {
        ScmlValue name = eval(vm, &ops[0]);
        char b[64];
        int ok = scml_vm_bind_event(vm, value_to_cstr(&name, b, sizeof(b)), (uint32_t)ops[1].i);
        value_free(&name);
        if (!ok) { error_at(vm, ins_pc, err, err_size, "event registry full"); free_decoded(ops, argc); return -1; }
        break;
    }
    case SCML_OP_EVENT_TRIGGER: {
        ScmlValue name = eval(vm, &ops[0]);
        char b[64];
        int ok = scml_vm_trigger_event(vm, value_to_cstr(&name, b, sizeof(b)), err, err_size);
        value_free(&name);
        if (!ok) { free_decoded(ops, argc); return -1; }
        break;
    }
    case SCML_OP_ENTITY_SPAWN: {
        ScmlValue args[4];
        args[0] = eval(vm, &ops[0]); args[1] = eval(vm, &ops[1]); args[2] = eval(vm, &ops[2]); args[3] = eval(vm, &ops[3]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "gpu.entity_spawn", args, 4, &ret);
        for (int i = 0; i < 4; i++) value_free(&args[i]);
        if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, "ENTITY_SPAWN requires gpu.entity_spawn"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[4].s, value_clone(&ret));
        value_free(&ret);
        break;
    }
    case SCML_OP_ENTITY_SET: {
        ScmlValue args[3];
        args[0] = eval(vm, &ops[0]);
        args[1] = eval(vm, &ops[1]);
        args[2] = eval(vm, &ops[2]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "gpu.entity_set", args, 3, &ret);
        for (int i = 0; i < 3; i++) value_free(&args[i]);
        value_free(&ret);
        if (!ok) { error_at(vm, ins_pc, err, err_size, "ENTITY_SET requires gpu.entity_set"); free_decoded(ops, argc); return -1; }
        break;
    }
    case SCML_OP_HEAP_ALLOC: case SCML_OP_ARRAY_CREATE: {
        ScmlValue sz = eval(vm, &ops[0]);
        int count = value_to_int(&sz);
        uint32_t ref = 0;
        if (count < 0) { value_free(&sz); error_at(vm, ins_pc, err, err_size, "negative heap allocation size"); free_decoded(ops, argc); return -1; }
        if (!heap_alloc(vm, (size_t)count, &ref)) { value_free(&sz); error_at(vm, ins_pc, err, err_size, "heap allocation failed"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[1].s, value_int((int32_t)ref));
        value_free(&sz);
        break;
    }
    case SCML_OP_FREE: {
        ScmlValue ref_value = eval(vm, &ops[0]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        if (obj) { free(obj->data); memset(obj, 0, sizeof(*obj)); }
        value_free(&ref_value);
        break;
    }
    case SCML_OP_HEAP_STORE: {
        ScmlValue ref_value = eval(vm, &ops[0]), index = eval(vm, &ops[1]), value = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        size_t i = (size_t)value_to_int(&index);
        if (!heap_store_i32(obj, i, value_to_int(&value))) { value_free(&ref_value); value_free(&index); value_free(&value); error_at(vm, ins_pc, err, err_size, "heap write out of range"); free_decoded(ops, argc); return -1; }
        value_free(&ref_value); value_free(&index); value_free(&value);
        break;
    }
    case SCML_OP_HEAP_LOAD: {
        ScmlValue ref_value = eval(vm, &ops[1]), index = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        size_t i = (size_t)value_to_int(&index);
        int32_t loaded = 0;
        if (!heap_load_i32(obj, i, &loaded)) { value_free(&ref_value); value_free(&index); error_at(vm, ins_pc, err, err_size, "heap read out of range"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[0].s, value_int(loaded));
        value_free(&ref_value); value_free(&index);
        break;
    }
    case SCML_OP_SPAN_CREATE: {
        ScmlValue sz = eval(vm, &ops[0]);
        int count = value_to_int(&sz);
        uint32_t ref = 0;
        if (count < 0) { value_free(&sz); error_at(vm, ins_pc, err, err_size, "negative span size"); free_decoded(ops, argc); return -1; }
        if (!heap_alloc_typed(vm, (size_t)count, 1, 0, &ref)) { value_free(&sz); error_at(vm, ins_pc, err, err_size, "span allocation failed"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[1].s, value_int((int32_t)ref));
        value_free(&sz);
        break;
    }
    case SCML_OP_SPAN_PIN: {
        ScmlValue ref_value = eval(vm, &ops[0]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        if (!obj) { value_free(&ref_value); error_at(vm, ins_pc, err, err_size, "span pin invalid reference"); free_decoded(ops, argc); return -1; }
        obj->pinned = 1;
        value_free(&ref_value);
        break;
    }
    case SCML_OP_SPAN_FILL: {
        ScmlValue ref_value = eval(vm, &ops[0]), start_value = eval(vm, &ops[1]), count_value = eval(vm, &ops[2]), fill_value = eval(vm, &ops[3]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        int start_i = value_to_int(&start_value), count_i = value_to_int(&count_value);
        if (start_i < 0 || count_i < 0 || !heap_fill(obj, (size_t)start_i, (size_t)count_i, value_to_int(&fill_value))) {
            value_free(&ref_value); value_free(&start_value); value_free(&count_value); value_free(&fill_value);
            error_at(vm, ins_pc, err, err_size, "span fill out of range"); free_decoded(ops, argc); return -1;
        }
        value_free(&ref_value); value_free(&start_value); value_free(&count_value); value_free(&fill_value);
        break;
    }
    case SCML_OP_SPAN_WRITE_U8: {
        ScmlValue ref_value = eval(vm, &ops[0]), index = eval(vm, &ops[1]), value = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        int byte_value = value_to_int(&value);
        size_t i = (size_t)value_to_int(&index);
        if (!obj || obj->elem_size != 1 || i >= obj->size || byte_value < 0 || byte_value > 255) {
            value_free(&ref_value); value_free(&index); value_free(&value);
            error_at(vm, ins_pc, err, err_size, "span u8 write out of range"); free_decoded(ops, argc); return -1;
        }
        obj->data[i] = (unsigned char)byte_value;
        value_free(&ref_value); value_free(&index); value_free(&value);
        break;
    }
    case SCML_OP_SPAN_READ_U8: {
        ScmlValue ref_value = eval(vm, &ops[1]), index = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        size_t i = (size_t)value_to_int(&index);
        if (!obj || obj->elem_size != 1 || i >= obj->size) { value_free(&ref_value); value_free(&index); error_at(vm, ins_pc, err, err_size, "span u8 read out of range"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[0].s, value_int(obj->data[i]));
        value_free(&ref_value); value_free(&index);
        break;
    }
    case SCML_OP_CONSOLE_RENDER_SPAN: {
        ScmlValue ref_value = eval(vm, &ops[0]), width_value = eval(vm, &ops[1]), height_value = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        int width = value_to_int(&width_value), height = value_to_int(&height_value);
        if (width <= 0 || height <= 0 || !obj || obj->elem_size != 1 || !obj->pinned || (size_t)width > SIZE_MAX / (size_t)height || (size_t)width * (size_t)height > obj->size) {
            value_free(&ref_value); value_free(&width_value); value_free(&height_value);
            error_at(vm, ins_pc, err, err_size, "console render requires pinned byte span in range"); free_decoded(ops, argc); return -1;
        }
        fputs("\033[H", stdout);
        for (int row = 0; row < height; row++) {
            fwrite(obj->data + (size_t)row * (size_t)width, 1, (size_t)width, stdout);
            fputc('\n', stdout);
        }
        value_free(&ref_value); value_free(&width_value); value_free(&height_value);
        break;
    }

    case SCML_OP_ASYNC_SPAWN: {
        uint32_t task_id = 0;
        AsyncTask *slot = NULL;
        if (ops[0].type != SCML_OPERAND_ADDRESS) { error_at(vm, ins_pc, err, err_size, "ASYNC_SPAWN requires label entry"); free_decoded(ops, argc); return -1; }
        for (size_t i = 0; i < SCML_ASYNC_TASKS_MAX; i++) {
            if (!vm->async_tasks[i].active || vm->async_tasks[i].done) { slot = &vm->async_tasks[i]; break; }
        }
        if (!slot) { error_at(vm, ins_pc, err, err_size, "async task table full"); free_decoded(ops, argc); return -1; }
        task_id = vm->next_task_id++;
        if (task_id == 0) task_id = vm->next_task_id++;
        slot->active = 1;
        slot->done = 0;
        slot->id = task_id;
        slot->entry_pc = (uint32_t)ops[0].i;
        if (!queue_async_task(vm, slot->entry_pc, task_id)) { slot->active = 0; error_at(vm, ins_pc, err, err_size, "async queue full"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[1].s, value_int((int32_t)task_id));
        break;
    }
    case SCML_OP_ASYNC_DONE: {
        ScmlValue task_value = eval(vm, &ops[0]);
        AsyncTask *task = async_task_find(vm, (uint32_t)value_to_int(&task_value));
        set_var(vm, ops[1].s, value_int((task && task->done) ? 1 : 0));
        value_free(&task_value);
        break;
    }
    case SCML_OP_TYPE_DECL:
        break;
    case SCML_OP_TYPE_ASSERT: {
        ScmlValue value = eval(vm, &ops[0]);
        ScmlValue expected = eval(vm, &ops[1]);
        const char *type_name = ops[1].type == SCML_OPERAND_STRING ? ops[1].s : NULL;
        char type_buf[32];
        if (!type_name) type_name = value_to_cstr(&expected, type_buf, sizeof(type_buf));
        int ok = 0;
        if (strcmp(type_name, "i32") == 0 || strcmp(type_name, "int") == 0 || strcmp(type_name, "integer") == 0 ||
            strcmp(type_name, "bool") == 0 || strcmp(type_name, "i8") == 0 || strcmp(type_name, "i16") == 0 ||
            strcmp(type_name, "i64") == 0 || strcmp(type_name, "u8") == 0 || strcmp(type_name, "u16") == 0 ||
            strcmp(type_name, "u32") == 0 || strcmp(type_name, "u64") == 0 || strcmp(type_name, "usize") == 0 ||
            strcmp(type_name, "isize") == 0 || strcmp(type_name, "ref") == 0 || strncmp(type_name, "array<", 6) == 0 ||
            strncmp(type_name, "vec<", 4) == 0 || strncmp(type_name, "option<", 7) == 0 || strncmp(type_name, "result<", 7) == 0) ok = value.type == SCML_VAL_INT;
        else if (strcmp(type_name, "f32") == 0 || strcmp(type_name, "f64") == 0 || strcmp(type_name, "float") == 0 || strcmp(type_name, "number") == 0) ok = value.type == SCML_VAL_FLOAT || value.type == SCML_VAL_INT;
        else if (strcmp(type_name, "str") == 0 || strcmp(type_name, "string") == 0 || strcmp(type_name, "text") == 0) ok = value.type == SCML_VAL_STRING;
        else if (strcmp(type_name, "any") == 0) ok = 1;
        set_var(vm, ops[2].s, value_int(ok));
        value_free(&value);
        value_free(&expected);
        break;
    }
    case SCML_OP_INPUT: {
        ScmlValue prompt = eval(vm, &ops[0]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, "input.read", &prompt, 1, &ret);
        value_free(&prompt);
        if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, "INPUT requires input.read"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[1].s, value_clone(&ret));
        value_free(&ret);
        break;
    }
    case SCML_OP_STRCAT: {
        ScmlValue a = eval(vm, &ops[1]), b = eval(vm, &ops[2]);
        char ab[128], bb[128];
        const char *as = value_to_cstr(&a, ab, sizeof(ab));
        const char *bs = value_to_cstr(&b, bb, sizeof(bb));
        size_t ln = strlen(as)+strlen(bs)+1;
        char *tmp=(char*)malloc(ln);
        if(!tmp){ value_free(&a); value_free(&b); error_at(vm, ins_pc, err, err_size, "out of memory"); free_decoded(ops, argc); return -1; }
        snprintf(tmp, ln, "%s%s", as, bs);
        set_var(vm, ops[0].s, value_str(tmp));
        free(tmp);
        value_free(&a); value_free(&b);
        break;
    }
    case SCML_OP_TO_INT: {
        ScmlValue v=eval(vm,&ops[1]);
        set_var(vm, ops[0].s, value_int(value_to_int(&v)));
        value_free(&v);
        break;
    }
    case SCML_OP_TO_FLOAT: {
        ScmlValue v=eval(vm,&ops[1]);
        set_var(vm, ops[0].s, value_float(value_to_float(&v)));
        value_free(&v);
        break;
    }
    case SCML_OP_BIT_AND: case SCML_OP_BIT_OR: case SCML_OP_BIT_XOR: case SCML_OP_SHL: case SCML_OP_SHR: {
        ScmlValue a = eval(vm, &ops[1]), b = eval(vm, &ops[2]);
        int x = value_to_int(&a), y = value_to_int(&b), r = 0;
        if ((op == SCML_OP_SHL || op == SCML_OP_SHR) && (y < 0 || y >= 31)) {
            value_free(&a); value_free(&b);
            error_at(vm, ins_pc, err, err_size, "invalid shift count");
            free_decoded(ops, argc);
            return -1;
        }
        if (op == SCML_OP_BIT_AND) r = x & y;
        else if (op == SCML_OP_BIT_OR) r = x | y;
        else if (op == SCML_OP_BIT_XOR) r = x ^ y;
        else if (op == SCML_OP_SHL) r = x << y;
        else r = x >> y;
        set_var(vm, ops[0].s, value_int(r));
        value_free(&a); value_free(&b);
        break;
    }
    case SCML_OP_BIT_NOT: {
        ScmlValue v = eval(vm, &ops[1]);
        set_var(vm, ops[0].s, value_int(~value_to_int(&v)));
        value_free(&v);
        break;
    }
    case SCML_OP_POW: {
        ScmlValue a = eval(vm, &ops[1]), b = eval(vm, &ops[2]);
        float p = powf(value_to_float(&a), value_to_float(&b));
        set_var(vm, ops[0].s, value_float(p));
        value_free(&a); value_free(&b);
        break;
    }
    case SCML_OP_STRLEN: {
        ScmlValue s = eval(vm, &ops[1]);
        char sb[256];
        set_var(vm, ops[0].s, value_int((int32_t)strlen(value_to_cstr(&s, sb, sizeof(sb)))));
        value_free(&s);
        break;
    }
    case SCML_OP_SUBSTR: {
        ScmlValue src = eval(vm, &ops[1]), start = eval(vm, &ops[2]), len = eval(vm, &ops[3]);
        char sb[512];
        const char *s = value_to_cstr(&src, sb, sizeof(sb));
        int st = value_to_int(&start), ln = value_to_int(&len);
        size_t sl = strlen(s);
        if (st < 0) st = 0;
        if (ln < 0) ln = 0;
        if ((size_t)st > sl) st = (int)sl;
        size_t start_index = (size_t)st;
        size_t max_len = sl - start_index;
        if ((size_t)ln > max_len) ln = (int)max_len;
        char *tmp = (char *)malloc((size_t)ln + 1);
        if (!tmp) { value_free(&src); value_free(&start); value_free(&len); error_at(vm, ins_pc, err, err_size, "out of memory"); free_decoded(ops, argc); return -1; }
        memcpy(tmp, s + start_index, (size_t)ln);
        tmp[ln] = '\0';
        set_var(vm, ops[0].s, value_str(tmp));
        free(tmp);
        value_free(&src); value_free(&start); value_free(&len);
        break;
    }
    case SCML_OP_ARRAY_LEN: {
        ScmlValue refv = eval(vm, &ops[1]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&refv));
        set_var(vm, ops[0].s, value_int(obj ? (int32_t)obj->size : 0));
        value_free(&refv);
        break;
    }
    case SCML_OP_CALL_NATIVE: {
        const char *name = ops[0].s;
        ScmlValue args[7];
        memset(args, 0, sizeof(args));
        for (uint8_t ai = 1; ai < argc; ai++) args[ai - 1] = eval(vm, &ops[ai]);
        ScmlValue ret = value_int(0);
        int ok = scml_vm_call_native(vm, name, args, argc - 1, &ret);
        for (uint8_t ai = 1; ai < argc; ai++) value_free(&args[ai - 1]);
        if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, scml_ffi_last_error()); free_decoded(ops, argc); return -1; }
        set_var(vm, "$RETVAL", value_clone(&ret));
        value_free(&ret);
        break;
    }
    default:
        error_at(vm, ins_pc, err, err_size, "unknown opcode");
        free_decoded(ops, argc);
        return -1;
    }

    free_decoded(ops, argc);
    return 1;
}

int scml_vm_update(ScmlVM *vm, char *err, size_t err_size) {
    return scml_vm_run(vm, err, err_size);
}

int scml_vm_run(ScmlVM *vm, char *err, size_t err_size) {
    for (;;) {
        int rc = scml_vm_step(vm, err, err_size);
        if (rc < 0) {
            fflush(stdout);
            return 0;
        }
        if (rc == 0) {
            fflush(stdout);
            return 1;
        }
    }
}
