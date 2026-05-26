#ifndef SCML_OPCODE_H
#define SCML_OPCODE_H

#include <stdint.h>

#define SCML_MAX_OPCODE 200
#define SCML_MAGIC 0x4C4D4353u /* SCML */
#define SCML_VERSION 4u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScmlOpcode {
    SCML_OP_NOP = 0x00,
    SCML_OP_HALT = 0x01,
    SCML_OP_PUSH_INT = 0x02,
    SCML_OP_PUSH_STR = 0x03,
    SCML_OP_STORE = 0x04,
    SCML_OP_LOAD = 0x05,
    SCML_OP_ADD = 0x06,
    SCML_OP_SUB = 0x07,
    SCML_OP_MUL = 0x08,
    SCML_OP_DIV = 0x09,
    SCML_OP_JMP = 0x0A,
    SCML_OP_WAIT = 0x0B,
    SCML_OP_IF_EQ = 0x0C,
    SCML_OP_IF_NE = 0x0D,
    SCML_OP_IF_GT = 0x0E,
    SCML_OP_IF_LT = 0x0F,
    SCML_OP_PRINT = 0x10,
    SCML_OP_LOG = 0x11,
    SCML_OP_FILE_READ = 0x12,
    SCML_OP_FILE_WRITE = 0x13,
    SCML_OP_ENTITY_SPAWN = 0x14,
    SCML_OP_ENTITY_SET = 0x15,
    SCML_OP_HEAP_ALLOC = 0x16,
    SCML_OP_HEAP_STORE = 0x17,
    SCML_OP_HEAP_LOAD = 0x18,
    SCML_OP_CALL = 0x19,
    SCML_OP_RETURN = 0x1A,
    SCML_OP_END_THREAD = 0x1B,
    SCML_OP_EVENT_BIND = 0x1C,
    SCML_OP_EVENT_TRIGGER = 0x1D,
    SCML_OP_FREE = 0x1E,
    SCML_OP_ARRAY_CREATE = 0x1F,
    SCML_OP_MOD = 0x20,
    SCML_OP_INPUT = 0x21,
    SCML_OP_STRCAT = 0x22,
    SCML_OP_TO_INT = 0x23,
    SCML_OP_TO_FLOAT = 0x24,
    SCML_OP_IF_GE = 0x25,
    SCML_OP_IF_LE = 0x26,
    SCML_OP_BIT_AND = 0x27,
    SCML_OP_BIT_OR = 0x28,
    SCML_OP_BIT_XOR = 0x29,
    SCML_OP_BIT_NOT = 0x2A,
    SCML_OP_SHL = 0x2B,
    SCML_OP_SHR = 0x2C,
    SCML_OP_POW = 0x2D,
    SCML_OP_STRLEN = 0x2E,
    SCML_OP_SUBSTR = 0x2F,
    SCML_OP_ARRAY_LEN = 0x30,
    SCML_OP_CALL_NATIVE = 0x31
} ScmlOpcode;

typedef enum ScmlOperandType {
    SCML_OPERAND_NONE = 0,
    SCML_OPERAND_INT = 1,
    SCML_OPERAND_STRING = 2,
    SCML_OPERAND_VAR = 3,
    SCML_OPERAND_ADDRESS = 4,
    SCML_OPERAND_FLOAT = 5
} ScmlOperandType;

typedef struct ScmlOpcodeInfo {
    uint16_t scm_code;
    ScmlOpcode opcode;
    const char *name;
    uint8_t min_args;
    uint8_t max_args;
} ScmlOpcodeInfo;

const ScmlOpcodeInfo *scml_opcode_from_scm_code(uint16_t scm_code);
const ScmlOpcodeInfo *scml_opcode_from_name(const char *name);
const ScmlOpcodeInfo *scml_opcode_info(ScmlOpcode opcode);
const char *scml_opcode_name(ScmlOpcode opcode);

#ifdef __cplusplus
}
#endif

#endif
