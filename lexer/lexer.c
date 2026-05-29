#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *scml_strndup(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static int add_token(ScmlTokenList *out, ScmlTokenType type, const char *start, size_t len, int line, int col) {
    if (out->count == out->capacity) {
        size_t nc = out->capacity ? out->capacity * 2 : 8;
        ScmlToken *ni = (ScmlToken *)realloc(out->items, nc * sizeof(*ni));
        if (!ni) return 0;
        out->items = ni;
        out->capacity = nc;
    }
    out->items[out->count].type = type;
    out->items[out->count].text = scml_strndup(start, len);
    out->items[out->count].line = line;
    out->items[out->count].column = col;
    if (!out->items[out->count].text) return 0;
    out->count++;
    return 1;
}

int scml_lex_line(const char *line, int line_no, ScmlTokenList *out, char *err, size_t err_size) {
    memset(out, 0, sizeof(*out));
    size_t i = 0;
    while (line[i]) {
        if (line[i] == ';' || (line[i] == '/' && line[i + 1] == '/')) break;
        if (isspace((unsigned char)line[i]) || line[i] == ',') { i++; continue; }
        int col = (int)i + 1;
        if (line[i] == ':') {
            if (!add_token(out, SCML_TOK_COLON, line + i, 1, line_no, col)) return 0;
            i++;
            continue;
        }
        if (line[i] == '@') {
            size_t s = ++i;
            while (isalnum((unsigned char)line[i]) || line[i] == '_') i++;
            if (s == i) { snprintf(err, err_size, "line %d: empty label reference", line_no); return 0; }
            if (!add_token(out, SCML_TOK_LABEL_REF, line + s, i - s, line_no, col)) return 0;
            continue;
        }
        if (line[i] == '"') {
            char buf[4096]; size_t bi = 0; i++;
            while (line[i] && line[i] != '"') {
                if (line[i] == '\\' && line[i + 1]) {
                    i++;
                    char c = line[i];
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == 'e') c = '\033';
                    else if (c == 'x' && isxdigit((unsigned char)line[i + 1]) && isxdigit((unsigned char)line[i + 2])) {
                        char hex[3] = { line[i + 1], line[i + 2], '\0' };
                        c = (char)strtol(hex, NULL, 16);
                        i += 2;
                    } else if (c >= '0' && c <= '7') {
                        int value = c - '0';
                        int digits = 1;
                        while (digits < 3 && line[i + 1] >= '0' && line[i + 1] <= '7') {
                            value = value * 8 + (line[i + 1] - '0');
                            i++;
                            digits++;
                        }
                        c = (char)value;
                    }
                    buf[bi++] = c;
                } else {
                    buf[bi++] = line[i];
                }
                if (bi + 1 >= sizeof(buf)) { snprintf(err, err_size, "line %d: string too long", line_no); return 0; }
                i++;
            }
            if (line[i] != '"') { snprintf(err, err_size, "line %d: unterminated string", line_no); return 0; }
            i++;
            if (!add_token(out, SCML_TOK_STRING, buf, bi, line_no, col)) return 0;
            continue;
        }
        if ((line[i] == '&' && line[i+1] == '&') || (line[i] == '|' && line[i+1] == '|') ||
            (line[i] == '=' && line[i+1] == '=') || (line[i] == '!' && line[i+1] == '=') ||
            (line[i] == '<' && line[i+1] == '=') || (line[i] == '>' && line[i+1] == '=') ||
            (line[i] == '<' && line[i+1] == '<') || (line[i] == '>' && line[i+1] == '>') ||
            (line[i] == ':' && line[i+1] == ':') || (line[i] == '-' && line[i+1] == '>') ||
            (line[i] == '+' && line[i+1] == '+') || (line[i] == '-' && line[i+1] == '-')) {
            if (!add_token(out, SCML_TOK_IDENTIFIER, line + i, 2, line_no, col)) return 0;
            i += 2;
            continue;
        }
        if (strchr("+-*/%&|^~!?()[]{}.<>,", line[i])) {
            if (!add_token(out, SCML_TOK_IDENTIFIER, line + i, 1, line_no, col)) return 0;
            i++;
            continue;
        }
        if ((line[i] == '+' || line[i] == '-') && line[i + 1] == '=') {
            if (!add_token(out, SCML_TOK_IDENTIFIER, line + i, 2, line_no, col)) return 0;
            i += 2;
            continue;
        }
        if (line[i] == '=') {
            if (!add_token(out, SCML_TOK_IDENTIFIER, line + i, 1, line_no, col)) return 0;
            i++;
            continue;
        }
        if (isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '-' || line[i] == '#' || line[i] == '$') {
            size_t s = i;
            while (isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '-' || line[i] == '#' || line[i] == '.' || line[i] == '$' || line[i] == '@') i++;
            ScmlTokenType type = SCML_TOK_IDENTIFIER;
            int all_hex = 1;
            int decimal_number = 1;
            int has_dot = 0;
            size_t start = (line[s] == '-') ? s + 1 : s;
            if (line[start] == '$') all_hex = 0;
            if (memchr(line + s, '@', i - s)) { all_hex = 0; decimal_number = 0; }
            for (size_t j = start; j < i; j++) {
                if (!isxdigit((unsigned char)line[j]) && !(line[j] == 'x' && j == start + 1 && line[start] == '0')) all_hex = 0;
                if (line[j] == '.') has_dot = 1;
                else if (!isdigit((unsigned char)line[j])) decimal_number = 0;
            }
            if ((isdigit((unsigned char)line[start]) && (all_hex || (has_dot && decimal_number))) || (line[s] == '-' && (all_hex || decimal_number))) type = SCML_TOK_NUMBER;
            if (!add_token(out, type, line + s, i - s, line_no, col)) return 0;
            continue;
        }
        snprintf(err, err_size, "line %d:%d: unexpected character '%c'", line_no, col, line[i]);
        return 0;
    }
    return 1;
}

void scml_token_list_free(ScmlTokenList *list) {
    for (size_t i = 0; i < list->count; i++) free(list->items[i].text);
    free(list->items);
    memset(list, 0, sizeof(*list));
}
