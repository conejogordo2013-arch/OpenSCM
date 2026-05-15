#include "vm.h"

#include "../opcode/opcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Var { char *name; ScmlValue value; } Var;
typedef struct HeapObject { int active; uint32_t id; size_t size; int32_t *data; } HeapObject;
typedef struct CallFrame { size_t return_pc; Var locals[SCML_LOCAL_VARS_MAX]; size_t local_count; } CallFrame;
typedef struct EventEntry { char *name; uint32_t handlers[SCML_EVENT_HANDLERS_MAX]; size_t handler_count; } EventEntry;
typedef struct EventQueueItem { char *name; size_t handler_index; } EventQueueItem;
typedef struct LineEntry { uint32_t pc; uint32_t line; } LineEntry;
typedef struct LabelEntry { char *name; uint32_t pc; } LabelEntry;
typedef struct NativeFunc { char *name; ScmlNativeFunc fn; void *user_data; } NativeFunc;

typedef struct DecOp { ScmlOperandType type; int32_t i; float f; char *s; } DecOp;

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
    ScmlEntity entities[SCML_ENTITIES_MAX];
    int next_entity_id;
    LineEntry *lines;
    size_t line_count;
    LabelEntry *labels;
    size_t label_count;
    NativeFunc natives[SCML_NATIVE_FUNCS_MAX];
    size_t native_count;
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
}

static ScmlValue value_int(int32_t x) {
    ScmlValue v;
    v.type = SCML_VAL_INT;
    v.integer = x;
    v.real = (float)x;
    v.string = NULL;
    return v;
}

static ScmlValue value_str(const char *s) {
    ScmlValue v;
    v.type = SCML_VAL_STRING;
    v.integer = 0;
    v.real = 0.0f;
    v.string = xstrdup(s ? s : "");
    return v;
}

static ScmlValue value_float(float x) {
    ScmlValue v;
    v.type = SCML_VAL_FLOAT;
    v.integer = (int32_t)x;
    v.real = x;
    v.string = NULL;
    return v;
}

static ScmlValue value_clone(const ScmlValue *v) {
    if (v->type == SCML_VAL_STRING) return value_str(v->string);
    if (v->type == SCML_VAL_FLOAT) return value_float(v->real);
    return value_int(v->integer);
}

static int value_to_int(const ScmlValue *v) {
    if (v->type == SCML_VAL_INT) return v->integer;
    if (v->type == SCML_VAL_FLOAT) return (int)v->real;
    return atoi(v->string ? v->string : "0");
}

static float value_to_float(const ScmlValue *v) {
    if (v->type == SCML_VAL_FLOAT) return v->real;
    if (v->type == SCML_VAL_INT) return (float)v->integer;
    return v->string ? (float)atof(v->string) : 0.0f;
}

static const char *value_to_cstr(const ScmlValue *v, char *buf, size_t n) {
    if (v->type == SCML_VAL_STRING) return v->string ? v->string : "";
    if (v->type == SCML_VAL_FLOAT) { snprintf(buf, n, "%g", v->real); return buf; }
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
    Var *v = &vars[(*count)++];
    v->name = xstrdup(name);
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

static HeapObject *heap_find(ScmlVM *vm, uint32_t ref) {
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) {
        if (vm->heap[i].active && vm->heap[i].id == ref) return &vm->heap[i];
    }
    return NULL;
}

static int heap_alloc(ScmlVM *vm, size_t size, uint32_t *out_ref) {
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) {
        if (!vm->heap[i].active) {
            vm->heap[i].data = (int32_t *)calloc(size ? size : 1, sizeof(int32_t));
            if (!vm->heap[i].data) return 0;
            vm->heap[i].active = 1;
            vm->heap[i].id = vm->next_ref++;
            vm->heap[i].size = size;
            *out_ref = vm->heap[i].id;
            return 1;
        }
    }
    return 0;
}


static NativeFunc *native_find(ScmlVM *vm, const char *name) {
    for (size_t i = 0; i < vm->native_count; i++) {
        if (strcmp(vm->natives[i].name, name) == 0) return &vm->natives[i];
    }
    return NULL;
}

static EventEntry *event_find(ScmlVM *vm, const char *name, int create) {
    for (size_t i = 0; i < vm->event_count; i++) if (strcmp(vm->events[i].name, name) == 0) return &vm->events[i];
    if (!create || vm->event_count >= SCML_EVENTS_MAX) return NULL;
    EventEntry *event = &vm->events[vm->event_count++];
    event->name = xstrdup(name);
    event->handler_count = 0;
    return event;
}

int scml_vm_bind_event(ScmlVM *vm, const char *event_name, uint32_t handler_pc) {
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
    vm->event_tail = next;
    return 1;
}

static int queue_event_handler(ScmlVM *vm, const char *event_name, size_t handler_index) {
    size_t next = (vm->event_tail + 1) % SCML_EVENT_QUEUE_MAX;
    if (next == vm->event_head) return 0;
    vm->event_queue[vm->event_tail].name = xstrdup(event_name);
    if (!vm->event_queue[vm->event_tail].name) return 0;
    vm->event_queue[vm->event_tail].handler_index = handler_index;
    vm->event_tail = next;
    return 1;
}

static int dispatch_next_event(ScmlVM *vm) {
    if (vm->event_head == vm->event_tail) return 0;
    char *event_name = vm->event_queue[vm->event_head].name;
    size_t handler_index = vm->event_queue[vm->event_head].handler_index;
    vm->event_head = (vm->event_head + 1) % SCML_EVENT_QUEUE_MAX;
    EventEntry *event = event_find(vm, event_name, 0);
    if (event && handler_index < event->handler_count) {
        if (handler_index + 1 < event->handler_count) queue_event_handler(vm, event_name, handler_index + 1);
        vm->pc = event->handlers[handler_index];
        vm->halted = 0;
        free(event_name);
        return 1;
    }
    free(event_name);
    return 0;
}

static void wait_ms(int ms) {
    clock_t end = clock() + (clock_t)((double)ms * CLOCKS_PER_SEC / 1000.0);
    while (clock() < end) {}
}

ScmlVM *scml_vm_create(void) {
    ScmlVM *vm = (ScmlVM *)calloc(1, sizeof(*vm));
    if (!vm) return NULL;
    vm->next_ref = 1;
    vm->next_entity_id = 1;
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
    NativeFunc *existing = native_find(vm, name);
    if (existing) { existing->fn = fn; existing->user_data = user_data; return 1; }
    if (vm->native_count >= SCML_NATIVE_FUNCS_MAX) return 0;
    NativeFunc *native = &vm->natives[vm->native_count++];
    native->name = xstrdup(name);
    native->fn = fn;
    native->user_data = user_data;
    return native->name != NULL;
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
    for (size_t i = 0; i < SCML_HEAP_OBJECTS_MAX; i++) if (vm->heap[i].active) fprintf(out, "  ref=%u size=%zu\n", vm->heap[i].id, vm->heap[i].size);
}

int scml_vm_step(ScmlVM *vm, char *err, size_t err_size) {
    if (vm->halted || vm->pc >= vm->code_size) {
        if (dispatch_next_event(vm)) return 1;
        vm->halted = 1;
        return 0;
    }

    size_t ins_pc = vm->pc;
    ScmlOpcode op = (ScmlOpcode)r8(vm);
    uint8_t argc = r8(vm);
    DecOp ops[8] = {0};
    if (argc > 8) { error_at(vm, ins_pc, err, err_size, "too many operands"); return -1; }
    for (uint8_t i = 0; i < argc; i++) {
        if (!decode_operand(vm, &ops[i], err, err_size)) { free_decoded(ops, i); return -1; }
    }
    if (vm->trace) fprintf(stderr, "[SCML] pc=%zu line=%u op=%s argc=%u\n", ins_pc, current_line(vm, ins_pc), scml_opcode_name(op), argc);

    switch (op) {
    case SCML_OP_NOP: break;
    case SCML_OP_HALT: vm->halted = 1; break;
    case SCML_OP_END_THREAD: vm->halted = !dispatch_next_event(vm); break;
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
            if (!native || !native->fn) { error_at(vm, ins_pc, err, err_size, "native function not registered"); free_decoded(ops, argc); return -1; }
            ScmlValue args[7];
            memset(args, 0, sizeof(args));
            for (uint8_t ai = 1; ai < argc; ai++) args[ai - 1] = eval(vm, &ops[ai]);
            ScmlValue ret = value_int(0);
            int ok = native->fn(vm, args, argc - 1, &ret, native->user_data);
            for (uint8_t ai = 1; ai < argc; ai++) value_free(&args[ai - 1]);
            if (!ok) { value_free(&ret); error_at(vm, ins_pc, err, err_size, "native function failed"); free_decoded(ops, argc); return -1; }
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
    case SCML_OP_ADD: case SCML_OP_SUB: case SCML_OP_MUL: case SCML_OP_DIV: {
        ScmlValue a = eval(vm, &ops[1]);
        ScmlValue b = eval(vm, &ops[2]);
        int x = value_to_int(&a), y = value_to_int(&b), r = 0;
        if (op == SCML_OP_ADD) r = x + y;
        else if (op == SCML_OP_SUB) r = x - y;
        else if (op == SCML_OP_MUL) r = x * y;
        else { if (y == 0) { value_free(&a); value_free(&b); error_at(vm, ins_pc, err, err_size, "division by zero"); free_decoded(ops, argc); return -1; } r = x / y; }
        value_free(&a);
        value_free(&b);
        set_var(vm, ops[0].s, value_int(r));
        break;
    }
    case SCML_OP_JMP:
        vm->pc = (size_t)ops[0].i;
        break;
    case SCML_OP_IF_EQ: case SCML_OP_IF_NE: case SCML_OP_IF_GT: case SCML_OP_IF_LT: {
        ScmlValue a = eval(vm, &ops[0]);
        ScmlValue b = eval(vm, &ops[1]);
        int x = value_to_int(&a), y = value_to_int(&b), take = 0;
        if (op == SCML_OP_IF_EQ) take = x == y;
        else if (op == SCML_OP_IF_NE) take = x != y;
        else if (op == SCML_OP_IF_GT) take = x > y;
        else take = x < y;
        value_free(&a);
        value_free(&b);
        if (take) vm->pc = (size_t)ops[2].i;
        break;
    }
    case SCML_OP_PRINT: case SCML_OP_LOG: {
        ScmlValue v = eval(vm, &ops[0]);
        char b[64];
        printf("%s\n", value_to_cstr(&v, b, sizeof(b)));
        value_free(&v);
        break;
    }
    case SCML_OP_WAIT: {
        ScmlValue v = eval(vm, &ops[0]);
        wait_ms(value_to_int(&v));
        value_free(&v);
        break;
    }
    case SCML_OP_FILE_READ: {
        ScmlValue path = eval(vm, &ops[0]);
        char path_buf[64];
        FILE *file = fopen(value_to_cstr(&path, path_buf, sizeof(path_buf)), "rb");
        char *data = xstrdup("");
        if (file) {
            fseek(file, 0, SEEK_END);
            long n = ftell(file);
            rewind(file);
            data = (char *)realloc(data, (size_t)n + 1);
            if (fread(data, 1, (size_t)n, file) == (size_t)n) data[n] = '\0'; else data[0] = '\0';
            fclose(file);
        }
        set_var(vm, ops[1].s, value_str(data));
        free(data);
        value_free(&path);
        break;
    }
    case SCML_OP_FILE_WRITE: {
        ScmlValue path = eval(vm, &ops[0]);
        ScmlValue data = eval(vm, &ops[1]);
        char path_buf[64], data_buf[64];
        FILE *file = fopen(value_to_cstr(&path, path_buf, sizeof(path_buf)), "ab");
        if (file) { fputs(value_to_cstr(&data, data_buf, sizeof(data_buf)), file); fputc('\n', file); fclose(file); }
        value_free(&path);
        value_free(&data);
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
        ScmlValue model = eval(vm, &ops[0]), x = eval(vm, &ops[1]), y = eval(vm, &ops[2]), z = eval(vm, &ops[3]);
        int id = vm->next_entity_id++;
        for (size_t i = 0; i < SCML_ENTITIES_MAX; i++) if (!vm->entities[i].active) {
            vm->entities[i].active = 1;
            vm->entities[i].id = id;
            snprintf(vm->entities[i].model, sizeof(vm->entities[i].model), "%s", model.type == SCML_VAL_STRING ? model.string : "entity");
            vm->entities[i].x = value_to_int(&x); vm->entities[i].y = value_to_int(&y); vm->entities[i].z = value_to_int(&z);
            break;
        }
        set_var(vm, ops[4].s, value_int(id));
        value_free(&model); value_free(&x); value_free(&y); value_free(&z);
        break;
    }
    case SCML_OP_ENTITY_SET:
        break;
    case SCML_OP_HEAP_ALLOC: case SCML_OP_ARRAY_CREATE: {
        ScmlValue sz = eval(vm, &ops[0]);
        uint32_t ref = 0;
        if (!heap_alloc(vm, (size_t)value_to_int(&sz), &ref)) { value_free(&sz); error_at(vm, ins_pc, err, err_size, "heap allocation failed"); free_decoded(ops, argc); return -1; }
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
        if (!obj || i >= obj->size) { value_free(&ref_value); value_free(&index); value_free(&value); error_at(vm, ins_pc, err, err_size, "heap write out of range"); free_decoded(ops, argc); return -1; }
        obj->data[i] = value_to_int(&value);
        value_free(&ref_value); value_free(&index); value_free(&value);
        break;
    }
    case SCML_OP_HEAP_LOAD: {
        ScmlValue ref_value = eval(vm, &ops[1]), index = eval(vm, &ops[2]);
        HeapObject *obj = heap_find(vm, (uint32_t)value_to_int(&ref_value));
        size_t i = (size_t)value_to_int(&index);
        if (!obj || i >= obj->size) { value_free(&ref_value); value_free(&index); error_at(vm, ins_pc, err, err_size, "heap read out of range"); free_decoded(ops, argc); return -1; }
        set_var(vm, ops[0].s, value_int(obj->data[i]));
        value_free(&ref_value); value_free(&index);
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
        if (rc < 0) return 0;
        if (rc == 0) return 1;
    }
}
