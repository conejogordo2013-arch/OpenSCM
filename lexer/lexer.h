#ifndef SCML_LEXER_H
#define SCML_LEXER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScmlTokenType {
    SCML_TOK_EOF,
    SCML_TOK_IDENTIFIER,
    SCML_TOK_NUMBER,
    SCML_TOK_STRING,
    SCML_TOK_LABEL_REF,
    SCML_TOK_COLON,
    SCML_TOK_COMMA
} ScmlTokenType;

typedef struct ScmlToken {
    ScmlTokenType type;
    char *text;
    int line;
    int column;
} ScmlToken;

typedef struct ScmlTokenList {
    ScmlToken *items;
    size_t count;
    size_t capacity;
} ScmlTokenList;

int scml_lex_line(const char *line, int line_no, ScmlTokenList *out, char *err, size_t err_size);
void scml_token_list_free(ScmlTokenList *list);

#ifdef __cplusplus
}
#endif

#endif
