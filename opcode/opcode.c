#include "opcode.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static const ScmlOpcodeInfo g_opcodes[] = {
    {0x0000, SCML_OP_NOP, "NOP", 0, 0},
    {0x0001, SCML_OP_HALT, "HALT", 0, 0},
    {0x0002, SCML_OP_PUSH_INT, "PUSH_INT", 1, 1},
    {0x0003, SCML_OP_PUSH_STR, "PUSH_STR", 1, 1},
    {0x0004, SCML_OP_STORE, "SET", 2, 2},
    {0x0005, SCML_OP_LOAD, "GET", 1, 1},
    {0x0006, SCML_OP_ADD, "ADD", 3, 3},
    {0x0007, SCML_OP_SUB, "SUB", 3, 3},
    {0x0008, SCML_OP_MUL, "MUL", 3, 3},
    {0x0009, SCML_OP_DIV, "DIV", 3, 3},
    {0x000A, SCML_OP_JMP, "JUMP", 1, 1},
    {0x000B, SCML_OP_WAIT, "WAIT", 1, 1},
    {0x00D6, SCML_OP_IF_EQ, "IF_EQ", 3, 3},
    {0x00D7, SCML_OP_IF_NE, "IF_NE", 3, 3},
    {0x00D8, SCML_OP_IF_GT, "IF_GT", 3, 3},
    {0x00D9, SCML_OP_IF_LT, "IF_LT", 3, 3},
    {0x03E5, SCML_OP_PRINT, "PRINT", 1, 1},
    {0x03E6, SCML_OP_LOG, "LOG", 1, 1},
    {0x0A10, SCML_OP_FILE_READ, "FILE_READ", 2, 2},
    {0x0A20, SCML_OP_FILE_WRITE, "FILE_WRITE", 2, 2},
    {0x0A30, SCML_OP_EVENT_BIND, "BIND_EVENT", 2, 2},
    {0x0A31, SCML_OP_EVENT_TRIGGER, "TRIGGER_EVENT", 1, 1},
    {0x0B00, SCML_OP_ENTITY_SPAWN, "ENTITY_SPAWN", 5, 5},
    {0x0B01, SCML_OP_ENTITY_SET, "ENTITY_SET", 3, 3},
    {0x0B10, SCML_OP_HEAP_ALLOC, "ALLOC", 2, 2},
    {0x0B11, SCML_OP_FREE, "FREE", 1, 1},
    {0x0B12, SCML_OP_HEAP_LOAD, "READ", 3, 3},
    {0x0B13, SCML_OP_HEAP_STORE, "WRITE", 3, 3},
    {0x0B14, SCML_OP_ARRAY_CREATE, "ARRAY_CREATE", 2, 2},
    {0x0B20, SCML_OP_MOD, "MOD", 3, 3},
    {0x0B21, SCML_OP_INPUT, "INPUT", 2, 2},
    {0x0B22, SCML_OP_STRCAT, "STRCAT", 3, 3},
    {0x0B23, SCML_OP_TO_INT, "TO_INT", 2, 2},
    {0x0B24, SCML_OP_TO_FLOAT, "TO_FLOAT", 2, 2},
    {0x0B25, SCML_OP_IF_GE, "IF_GE", 3, 3},
    {0x0B26, SCML_OP_IF_LE, "IF_LE", 3, 3},
    {0x0B27, SCML_OP_BIT_AND, "BIT_AND", 3, 3},
    {0x0B28, SCML_OP_BIT_OR, "BIT_OR", 3, 3},
    {0x0B29, SCML_OP_BIT_XOR, "BIT_XOR", 3, 3},
    {0x0B2A, SCML_OP_BIT_NOT, "BIT_NOT", 2, 2},
    {0x0B2B, SCML_OP_SHL, "SHL", 3, 3},
    {0x0B2C, SCML_OP_SHR, "SHR", 3, 3},
    {0x0B2D, SCML_OP_POW, "POW", 3, 3},
    {0x0B2E, SCML_OP_STRLEN, "STRLEN", 2, 2},
    {0x0B2F, SCML_OP_SUBSTR, "SUBSTR", 4, 4},
    {0x0B30, SCML_OP_ARRAY_LEN, "ARRAY_LEN", 2, 2},
    {0x0B31, SCML_OP_CALL_NATIVE, "CALL_NATIVE", 1, 8},
    {0x0B32, SCML_OP_PRINT_RAW, "PRINT_RAW", 1, 1},
    {0x0B33, SCML_OP_CONSOLE_CLEAR, "CONSOLE_CLEAR", 0, 0},
    {0x0B34, SCML_OP_SIN, "SIN", 2, 2},
    {0x0B35, SCML_OP_COS, "COS", 2, 2},
    {0x0B36, SCML_OP_TAN, "TAN", 2, 2},
    {0x0B37, SCML_OP_SQRT, "SQRT", 2, 2},
    {0x0B38, SCML_OP_ATAN2, "ATAN2", 3, 3},
    {0x0B39, SCML_OP_FLOOR, "FLOOR", 2, 2},
    {0x0B3A, SCML_OP_CEIL, "CEIL", 2, 2},
    {0x0B3B, SCML_OP_ROUND, "ROUND", 2, 2},
    {0x0B3C, SCML_OP_ABS, "ABS", 2, 2},
    {0x0B3D, SCML_OP_STR_REPEAT, "STR_REPEAT", 3, 3},
    {0x0B3E, SCML_OP_CONSOLE_COLOR, "CONSOLE_COLOR", 1, 2},
    {0x0B3F, SCML_OP_CONSOLE_RESET, "CONSOLE_RESET", 0, 0},
    {0x0B40, SCML_OP_CONSOLE_MOVE, "CONSOLE_MOVE", 2, 2},
    {0x0B41, SCML_OP_CONSOLE_ERASE_LINE, "CONSOLE_ERASE_LINE", 0, 0},
    {0x0B42, SCML_OP_CONSOLE_STYLE, "CONSOLE_STYLE", 1, 1},
    {0x0B43, SCML_OP_SPAN_CREATE, "SPAN_CREATE", 2, 2},
    {0x0B44, SCML_OP_SPAN_PIN, "SPAN_PIN", 1, 1},
    {0x0B45, SCML_OP_SPAN_FILL, "SPAN_FILL", 4, 4},
    {0x0B46, SCML_OP_SPAN_WRITE_U8, "SPAN_WRITE_U8", 3, 3},
    {0x0B47, SCML_OP_SPAN_READ_U8, "SPAN_READ_U8", 3, 3},
    {0x0B48, SCML_OP_CONSOLE_RENDER_SPAN, "CONSOLE_RENDER_SPAN", 3, 3},
    {0x0B49, SCML_OP_ASYNC_SPAWN, "ASYNC_SPAWN", 2, 2},
    {0x0B4A, SCML_OP_ASYNC_DONE, "ASYNC_DONE", 2, 2},
    {0x0B4B, SCML_OP_TYPE_DECL, "TYPE_DECL", 2, 2},
    {0x0B4C, SCML_OP_TYPE_ASSERT, "TYPE_ASSERT", 3, 3},
    {0x0C00, SCML_OP_HEAP_ALLOC, "HEAP_ALLOC", 2, 2},
    {0x0C01, SCML_OP_HEAP_STORE, "HEAP_STORE", 3, 3},
    {0x0C02, SCML_OP_HEAP_LOAD, "HEAP_LOAD", 3, 3},
    {0x0D00, SCML_OP_CALL, "CALL", 1, 8},
    {0x0D00, SCML_OP_CALL, "GOSUB", 1, 1},
    {0x000A, SCML_OP_JMP, "GOTO", 1, 1},
    {0x0D01, SCML_OP_RETURN, "RETURN", 0, 0},
    {0x0D02, SCML_OP_END_THREAD, "END_THREAD", 0, 0},
};

static int eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

const ScmlOpcodeInfo *scml_opcode_from_scm_code(uint16_t scm_code) {
    for (size_t i = 0; i < sizeof(g_opcodes) / sizeof(g_opcodes[0]); i++) {
        if (g_opcodes[i].scm_code == scm_code) return &g_opcodes[i];
    }
    return NULL;
}

const ScmlOpcodeInfo *scml_opcode_from_name(const char *name) {
    for (size_t i = 0; i < sizeof(g_opcodes) / sizeof(g_opcodes[0]); i++) {
        if (eq_ci(g_opcodes[i].name, name)) return &g_opcodes[i];
    }
    return NULL;
}

const ScmlOpcodeInfo *scml_opcode_info(ScmlOpcode opcode) {
    for (size_t i = 0; i < sizeof(g_opcodes) / sizeof(g_opcodes[0]); i++) {
        if (g_opcodes[i].opcode == opcode) return &g_opcodes[i];
    }
    return NULL;
}

const char *scml_opcode_name(ScmlOpcode opcode) {
    const ScmlOpcodeInfo *info = scml_opcode_info(opcode);
    return info ? info->name : "UNKNOWN";
}
