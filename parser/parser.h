#ifndef SCML_PARSER_H
#define SCML_PARSER_H
#include <stddef.h>
#include "../opcode/opcode.h"
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct ScmlOperand {
    ScmlOperandType type;
    char *text;
    int64_t integer;
    float real;
} ScmlOperand;

typedef struct ScmlStatement {
    int line;
    char *label;
    ScmlOpcode opcode;
    ScmlOperand operands[8];
    size_t operand_count;
} ScmlStatement;

typedef struct ScmlProgram {
    ScmlStatement *items;
    size_t count;
    size_t capacity;
} ScmlProgram;

int scml_parse_file(const char *path, ScmlProgram *program, char *err, size_t err_size);
int scml_preprocess_file(const char *path, char **out_text, char *err, size_t err_size);
void scml_program_free(ScmlProgram *program);
#ifdef __cplusplus
}
#endif
#endif
