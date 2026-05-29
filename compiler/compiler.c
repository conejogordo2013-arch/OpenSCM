#include "compiler.h"

#include "../opcode/opcode.h"
#include "../parser/parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LabelAddr {
    char *name;
    uint32_t addr;
} LabelAddr;

typedef struct LineAddr {
    uint32_t addr;
    uint32_t line;
} LineAddr;

typedef struct ProgramSet {
    ScmlProgram *programs;
    size_t count;
} ProgramSet;

typedef struct TypeBinding {
    char *name;
    char type[16];
} TypeBinding;

static const char *operand_static_type(const ScmlOperand *o, TypeBinding *types, size_t type_count) {
    if (!o) return "any";
    if (o->type == SCML_OPERAND_INT || o->type == SCML_OPERAND_ADDRESS) return "i32";
    if (o->type == SCML_OPERAND_FLOAT) return "f32";
    if (o->type == SCML_OPERAND_STRING) return "str";
    if (o->type == SCML_OPERAND_VAR) {
        for (size_t i = 0; i < type_count; i++) if (strcmp(types[i].name, o->text) == 0) return types[i].type;
    }
    return "any";
}

static int type_compatible(const char *expected, const char *actual) {
    if (!expected || !actual) return 1;
    if (strcmp(expected, "any") == 0 || strcmp(actual, "any") == 0) return 1;
    if (strcmp(expected, actual) == 0) return 1;
    if (strcmp(expected, "f32") == 0 && strcmp(actual, "i32") == 0) return 1;
    return 0;
}

static int set_type_binding(TypeBinding **types, size_t *type_count, const char *name, const char *type, char *err, size_t err_size) {
    if (!name || !type) return 0;
    for (size_t i = 0; i < *type_count; i++) {
        if (strcmp((*types)[i].name, name) == 0) {
            snprintf((*types)[i].type, sizeof((*types)[i].type), "%s", type);
            return 1;
        }
    }
    TypeBinding *next = (TypeBinding *)realloc(*types, (*type_count + 1) * sizeof(*next));
    if (!next) { snprintf(err, err_size, "out of memory"); return 0; }
    *types = next;
    size_t name_len = strlen(name);
    (*types)[*type_count].name = (char *)malloc(name_len + 1);
    if (!(*types)[*type_count].name) { snprintf(err, err_size, "out of memory"); return 0; }
    memcpy((*types)[*type_count].name, name, name_len + 1);
    snprintf((*types)[*type_count].type, sizeof((*types)[*type_count].type), "%s", type);
    (*type_count)++;
    return 1;
}

static void free_type_bindings(TypeBinding *types, size_t type_count) {
    for (size_t i = 0; i < type_count; i++) free(types[i].name);
    free(types);
}

static int validate_static_types(const ProgramSet *set, char *err, size_t err_size) {
    TypeBinding *types = NULL;
    size_t type_count = 0;
    int ok = 1;
    for (size_t p = 0; p < set->count && ok; p++) {
        for (size_t i = 0; i < set->programs[p].count && ok; i++) {
            ScmlStatement *stmt = &set->programs[p].items[i];
            if (stmt->label) continue;
            if (stmt->opcode == SCML_OP_TYPE_DECL) {
                const char *type = stmt->operands[1].text;
                if (stmt->operands[0].type != SCML_OPERAND_VAR || stmt->operands[1].type != SCML_OPERAND_STRING) {
                    snprintf(err, err_size, "line %d: TYPE_DECL expects variable and string type", stmt->line); ok = 0; break;
                }
                ok = set_type_binding(&types, &type_count, stmt->operands[0].text, type, err, err_size);
            } else if (stmt->opcode == SCML_OP_STORE && stmt->operand_count >= 2) {
                const char *expected = operand_static_type(&stmt->operands[0], types, type_count);
                const char *actual = operand_static_type(&stmt->operands[1], types, type_count);
                if (!type_compatible(expected, actual)) { snprintf(err, err_size, "line %d: static type mismatch assigning %s to %s", stmt->line, actual, expected); ok = 0; }
            } else if ((stmt->opcode == SCML_OP_ADD || stmt->opcode == SCML_OP_SUB || stmt->opcode == SCML_OP_MUL || stmt->opcode == SCML_OP_DIV || stmt->opcode == SCML_OP_MOD) && stmt->operand_count >= 3) {
                const char *dst = operand_static_type(&stmt->operands[0], types, type_count);
                const char *a = operand_static_type(&stmt->operands[1], types, type_count);
                const char *b = operand_static_type(&stmt->operands[2], types, type_count);
                const char *result = (strcmp(a, "f32") == 0 || strcmp(b, "f32") == 0) && stmt->opcode != SCML_OP_MOD ? "f32" : "i32";
                if ((!type_compatible("f32", a) && !type_compatible("i32", a)) || (!type_compatible("f32", b) && !type_compatible("i32", b)) || !type_compatible(dst, result)) {
                    snprintf(err, err_size, "line %d: static numeric type mismatch", stmt->line); ok = 0;
                }
            }
        }
    }
    free_type_bindings(types, type_count);
    return ok;
}


static void w8(FILE *f, uint8_t v) { fputc(v, f); }
static void w16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((v >> (i * 8)) & 255, f); }
static void wf32(FILE *f, float value) { union { float f; uint32_t u; } cvt; cvt.f = value; w32(f, cvt.u); }

static uint32_t stmt_size(const ScmlStatement *s) {
    if (s->label) return 0;
    uint32_t n = 2;
    for (size_t i = 0; i < s->operand_count; i++) {
        n += 1;
        if (s->operands[i].type == SCML_OPERAND_INT || s->operands[i].type == SCML_OPERAND_ADDRESS || s->operands[i].type == SCML_OPERAND_FLOAT) {
            n += 4;
        } else {
            n += 2 + (uint32_t)strlen(s->operands[i].text);
        }
    }
    return n;
}

static LabelAddr *find_label(LabelAddr *labels, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(labels[i].name, name) == 0) return &labels[i];
    }
    return NULL;
}

static int resolve_value(const ScmlOperand *o, LabelAddr *labels, size_t label_count, uint32_t *out, char *err, size_t err_size) {
    if (o->type == SCML_OPERAND_ADDRESS) {
        LabelAddr *label = find_label(labels, label_count, o->text);
        if (!label) {
            snprintf(err, err_size, "unresolved label @%s", o->text);
            return 0;
        }
        *out = label->addr;
        return 1;
    }
    *out = (uint32_t)o->integer;
    return 1;
}

static void free_program_set(ProgramSet *set) {
    if (!set) return;
    for (size_t i = 0; i < set->count; i++) scml_program_free(&set->programs[i]);
    free(set->programs);
    set->programs = NULL;
    set->count = 0;
}

int scml_compile_files(size_t source_count, const char **source_paths, const char *output_path, char *err, size_t err_size) {
    ProgramSet set = {0};
    LabelAddr *labels = NULL;
    LineAddr *lines = NULL;
    size_t label_count = 0;
    size_t line_count = 0;
    uint32_t pc = 0;

    set.programs = (ScmlProgram *)calloc(source_count, sizeof(*set.programs));
    if (!set.programs) {
        snprintf(err, err_size, "out of memory");
        return 0;
    }
    set.count = source_count;

    for (size_t i = 0; i < source_count; i++) {
        if (!scml_parse_file(source_paths[i], &set.programs[i], err, err_size)) {
            free_program_set(&set);
            return 0;
        }
    }

    if (!validate_static_types(&set, err, err_size)) {
        free_program_set(&set);
        return 0;
    }

    for (size_t p = 0; p < set.count; p++) {
        for (size_t i = 0; i < set.programs[p].count; i++) {
            ScmlStatement *stmt = &set.programs[p].items[i];
            if (stmt->label) {
                if (find_label(labels, label_count, stmt->label)) {
                    snprintf(err, err_size, "duplicate label :%s", stmt->label);
                    free(labels);
                    free(lines);
                    free_program_set(&set);
                    return 0;
                }
                labels = (LabelAddr *)realloc(labels, (label_count + 1) * sizeof(*labels));
                labels[label_count].name = stmt->label;
                labels[label_count].addr = pc;
                label_count++;
            } else {
                lines = (LineAddr *)realloc(lines, (line_count + 1) * sizeof(*lines));
                lines[line_count].addr = pc;
                lines[line_count].line = (uint32_t)stmt->line;
                line_count++;
                pc += stmt_size(stmt);
            }
        }
    }

    FILE *f = fopen(output_path, "wb");
    if (!f) {
        snprintf(err, err_size, "cannot create %s", output_path);
        free(labels);
        free(lines);
        free_program_set(&set);
        return 0;
    }

    w32(f, SCML_MAGIC);
    w16(f, SCML_VERSION);
    w32(f, pc);
    w32(f, (uint32_t)line_count);
    w32(f, (uint32_t)label_count);

    for (size_t p = 0; p < set.count; p++) {
        for (size_t i = 0; i < set.programs[p].count; i++) {
            ScmlStatement *stmt = &set.programs[p].items[i];
            if (stmt->label) continue;
            w8(f, (uint8_t)stmt->opcode);
            w8(f, (uint8_t)stmt->operand_count);
            for (size_t j = 0; j < stmt->operand_count; j++) {
                ScmlOperand *o = &stmt->operands[j];
                w8(f, (uint8_t)o->type);
                if (o->type == SCML_OPERAND_FLOAT) {
                    wf32(f, o->real);
                } else if (o->type == SCML_OPERAND_INT || o->type == SCML_OPERAND_ADDRESS) {
                    uint32_t v = 0;
                    if (!resolve_value(o, labels, label_count, &v, err, err_size)) {
                        fclose(f);
                        remove(output_path);
                        free(labels);
                        free(lines);
                        free_program_set(&set);
                        return 0;
                    }
                    w32(f, v);
                } else {
                    size_t n = strlen(o->text);
                    if (n > UINT16_MAX) {
                        snprintf(err, err_size, "string/variable too long");
                        fclose(f);
                        remove(output_path);
                        free(labels);
                        free(lines);
                        free_program_set(&set);
                        return 0;
                    }
                    w16(f, (uint16_t)n);
                    fwrite(o->text, 1, n, f);
                }
            }
        }
    }

    for (size_t i = 0; i < line_count; i++) {
        w32(f, lines[i].addr);
        w32(f, lines[i].line);
    }

    for (size_t i = 0; i < label_count; i++) {
        size_t n = strlen(labels[i].name);
        if (n > UINT16_MAX) {
            snprintf(err, err_size, "label too long");
            fclose(f);
            remove(output_path);
            free(labels);
            free(lines);
            free_program_set(&set);
            return 0;
        }
        w32(f, labels[i].addr);
        w16(f, (uint16_t)n);
        fwrite(labels[i].name, 1, n, f);
    }

    fclose(f);
    free(labels);
    free(lines);
    free_program_set(&set);
    return 1;
}

int scml_compile_file(const char *source_path, const char *output_path, char *err, size_t err_size) {
    return scml_compile_files(1, &source_path, output_path, err, err_size);
}
