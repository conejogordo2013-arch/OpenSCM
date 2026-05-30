#include "scml_ffi.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(SCML_USE_LIBFFI)
#include <ffi.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define SCML_FFI_MAYBE_UNUSED __attribute__((unused))
#else
#define SCML_FFI_MAYBE_UNUSED
#endif

struct ScmlFFILibrary {
    char *path;
    void *handle;
    unsigned int flags;
    size_t ref_count;
};

typedef struct ScmlFFISymbol {
    char *name;
    void *library_handle;
    void *function_ptr;
} ScmlFFISymbol;

typedef struct ScmlFFISignatureEntry {
    char *name;
    ScmlFFISignature signature;
    ScmlFFIType *arg_types;
} ScmlFFISignatureEntry;

typedef struct ScmlFFISearchPath {
    char *path;
} ScmlFFISearchPath;

typedef struct ScmlFFIMemoryBlock {
    void *ptr;
    size_t size;
} ScmlFFIMemoryBlock;

static ScmlFFILibrary *g_libraries = NULL;
static size_t g_library_count = 0;
static size_t g_library_capacity = 0;
static ScmlFFISymbol *g_symbols = NULL;
static size_t g_symbol_count = 0;
static size_t g_symbol_capacity = 0;
static ScmlFFISignatureEntry *g_signatures = NULL;
static size_t g_signature_count = 0;
static size_t g_signature_capacity = 0;
static ScmlFFISearchPath *g_search_paths = NULL;
static size_t g_search_path_count = 0;
static size_t g_search_path_capacity = 0;
static ScmlFFIMemoryBlock *g_memory_blocks = NULL;
static size_t g_memory_block_count = 0;
static size_t g_memory_block_capacity = 0;
static char g_last_error[512];

static char *ffi_strdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s ? s : "", n + 1);
    return r;
}

static void ffi_set_error(const char *prefix, const char *detail) {
    snprintf(g_last_error, sizeof(g_last_error), "%s%s%s", prefix ? prefix : "FFI error", detail ? ": " : "", detail ? detail : "");
}

const char *scml_ffi_last_error(void) { return g_last_error[0] ? g_last_error : "no FFI error"; }

static void ffi_clear_error(void) { g_last_error[0] = '\0'; }

static int ffi_grow_array(void **array_ptr, size_t elem_size, size_t *capacity, size_t min_capacity, size_t initial_capacity) {
    if (*capacity >= min_capacity) return 1;
    size_t next = *capacity ? *capacity : initial_capacity;
    while (next < min_capacity) {
        if (next > ((size_t)-1) / 2) { ffi_set_error("registry growth failed", "capacity overflow"); return 0; }
        next *= 2;
    }
    void *new_array = realloc(*array_ptr, next * elem_size);
    if (!new_array) { ffi_set_error("registry growth failed", "out of memory"); return 0; }
    *array_ptr = new_array;
    *capacity = next;
    return 1;
}

static int ffi_ensure_library_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_libraries, sizeof(g_libraries[0]), &g_library_capacity, min_capacity, SCML_FFI_INITIAL_LIBRARIES);
}

static int ffi_ensure_symbol_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_symbols, sizeof(g_symbols[0]), &g_symbol_capacity, min_capacity, SCML_FFI_INITIAL_SYMBOLS);
}

static int ffi_ensure_signature_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_signatures, sizeof(g_signatures[0]), &g_signature_capacity, min_capacity, SCML_FFI_INITIAL_SIGNATURES);
}

static int ffi_ensure_search_path_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_search_paths, sizeof(g_search_paths[0]), &g_search_path_capacity, min_capacity, SCML_FFI_INITIAL_SEARCH_PATHS);
}

static int ffi_ensure_memory_block_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_memory_blocks, sizeof(g_memory_blocks[0]), &g_memory_block_capacity, min_capacity, SCML_FFI_INITIAL_SYMBOLS);
}

static unsigned int ffi_normalize_load_flags(unsigned int flags) {
    if ((flags & (SCML_FFI_LOAD_NOW | SCML_FFI_LOAD_LAZY)) == 0) flags |= SCML_FFI_LOAD_NOW;
    if ((flags & (SCML_FFI_LOAD_LOCAL | SCML_FFI_LOAD_GLOBAL)) == 0) flags |= SCML_FFI_LOAD_LOCAL;
    return flags;
}

static void *ffi_platform_load(const char *path, unsigned int flags) {
#if defined(_WIN32)
    (void)flags;
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        char buf[64];
        snprintf(buf, sizeof(buf), "GetLastError=%lu", (unsigned long)GetLastError());
        ffi_set_error("LoadLibrary failed", buf);
    }
    return (void *)h;
#else
    int dl_flags = 0;
    flags = ffi_normalize_load_flags(flags);
    dl_flags |= (flags & SCML_FFI_LOAD_LAZY) ? RTLD_LAZY : RTLD_NOW;
#ifdef RTLD_GLOBAL
    dl_flags |= (flags & SCML_FFI_LOAD_GLOBAL) ? RTLD_GLOBAL : RTLD_LOCAL;
#endif
    dlerror();
    void *h = dlopen(path, dl_flags);
    if (!h) ffi_set_error("dlopen failed", dlerror());
    return h;
#endif
}

static int ffi_platform_unload(void *handle) {
#if defined(_WIN32)
    if (!FreeLibrary((HMODULE)handle)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "GetLastError=%lu", (unsigned long)GetLastError());
        ffi_set_error("FreeLibrary failed", buf);
        return 0;
    }
    return 1;
#else
    if (dlclose(handle) != 0) {
        ffi_set_error("dlclose failed", dlerror());
        return 0;
    }
    return 1;
#endif
}

static void *ffi_platform_symbol(void *handle, const char *name) {
#if defined(_WIN32)
    void *p = (void *)GetProcAddress((HMODULE)handle, name);
    if (!p) {
        char buf[64];
        snprintf(buf, sizeof(buf), "GetLastError=%lu", (unsigned long)GetLastError());
        ffi_set_error("GetProcAddress failed", buf);
    }
    return p;
#else
    dlerror();
    void *p = dlsym(handle, name);
    const char *e = dlerror();
    if (e) { ffi_set_error("dlsym failed", e); return NULL; }
    return p;
#endif
}

static int ffi_path_has_separator(const char *path) {
    if (!path) return 0;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') return 1;
    return 0;
}

static const char *ffi_platform_extension(void) {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

static int ffi_path_has_extension(const char *path) {
    if (!path) return 0;
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = slash && backslash ? (slash > backslash ? slash + 1 : backslash + 1) : (slash ? slash + 1 : (backslash ? backslash + 1 : path));
    return strchr(base, '.') != NULL;
}

static char *ffi_join_path(const char *dir, const char *name, int append_extension) {
    const char *ext = append_extension ? ffi_platform_extension() : "";
    size_t dn = strlen(dir ? dir : "");
    size_t nn = strlen(name ? name : "");
    size_t en = strlen(ext);
    int need_sep = dn > 0 && dir[dn - 1] != '/' && dir[dn - 1] != '\\';
    char *out = (char *)malloc(dn + (size_t)need_sep + nn + en + 1);
    if (!out) return NULL;
    memcpy(out, dir, dn);
    if (need_sep) out[dn++] = '/';
    memcpy(out + dn, name, nn);
    memcpy(out + dn + nn, ext, en);
    out[dn + nn + en] = '\0';
    return out;
}

static void *ffi_try_load_candidate(const char *candidate, unsigned int flags, char **loaded_path) {
    void *handle = ffi_platform_load(candidate, flags);
    if (!handle) return NULL;
    *loaded_path = ffi_strdup(candidate);
    if (!*loaded_path) {
        ffi_platform_unload(handle);
        ffi_set_error("load_library failed", "out of memory");
        return NULL;
    }
    return handle;
}

static void *ffi_load_with_search_paths(const char *path, unsigned int flags, char **loaded_path) {
    void *handle = ffi_try_load_candidate(path, flags, loaded_path);
    if (handle) return handle;
    int tried_candidates = 1;

    if (!ffi_path_has_extension(path)) {
        char *with_ext = ffi_join_path("", path, 1);
        if (!with_ext) { ffi_set_error("load_library failed", "out of memory"); return NULL; }
        handle = ffi_try_load_candidate(with_ext, flags, loaded_path);
        tried_candidates++;
        free(with_ext);
        if (handle) return handle;
    }

    if (!ffi_path_has_separator(path)) {
        for (size_t i = 0; i < g_search_path_count; i++) {
            char *candidate = ffi_join_path(g_search_paths[i].path, path, 0);
            if (!candidate) { ffi_set_error("load_library failed", "out of memory"); return NULL; }
            handle = ffi_try_load_candidate(candidate, flags, loaded_path);
            tried_candidates++;
            free(candidate);
            if (handle) return handle;
            if (!ffi_path_has_extension(path)) {
                candidate = ffi_join_path(g_search_paths[i].path, path, 1);
                if (!candidate) { ffi_set_error("load_library failed", "out of memory"); return NULL; }
                handle = ffi_try_load_candidate(candidate, flags, loaded_path);
                tried_candidates++;
                free(candidate);
                if (handle) return handle;
            }
        }
    }
    char detail[128];
    snprintf(detail, sizeof(detail), "no candidate matched after %d attempt(s)", tried_candidates);
    ffi_set_error("load_library failed", detail);
    return NULL;
}

int scml_ffi_add_search_path(const char *path) {
    if (!path || !path[0]) { ffi_set_error("add_search_path failed", "empty path"); return 0; }
    for (size_t i = 0; i < g_search_path_count; i++) {
        if (strcmp(g_search_paths[i].path, path) == 0) return 1;
    }
    if (!ffi_ensure_search_path_capacity(g_search_path_count + 1)) return 0;
    char *owned_path = ffi_strdup(path);
    if (!owned_path) { ffi_set_error("add_search_path failed", "out of memory"); return 0; }
    g_search_paths[g_search_path_count++].path = owned_path;
    return 1;
}

void *scml_ffi_load_library(const char *path) {
    return scml_ffi_load_library_ex(path, SCML_FFI_LOAD_NOW | SCML_FFI_LOAD_LOCAL);
}

void *scml_ffi_load_library_ex(const char *path, unsigned int flags) {
    if (!path || !path[0]) { ffi_set_error("load_library failed", "empty path"); return NULL; }
    flags = ffi_normalize_load_flags(flags);
    for (size_t i = 0; i < g_library_count; i++) {
        if (g_libraries[i].path && strcmp(g_libraries[i].path, path) == 0) {
            g_libraries[i].ref_count++;
            return g_libraries[i].handle;
        }
    }
    if (!ffi_ensure_library_capacity(g_library_count + 1)) return NULL;
    ffi_clear_error();
    char *owned_path = NULL;
    void *handle = ffi_load_with_search_paths(path, flags, &owned_path);
    if (!handle) return NULL;
    g_libraries[g_library_count].path = owned_path;
    g_libraries[g_library_count].handle = handle;
    g_libraries[g_library_count].flags = flags;
    g_libraries[g_library_count].ref_count = 1;
    g_library_count++;
    return handle;
}

int scml_ffi_unload_library(void *handle) {
    if (!handle) { ffi_set_error("unload_library failed", "null handle"); return 0; }
    for (size_t i = 0; i < g_library_count; i++) {
        if (g_libraries[i].handle == handle) {
            if (g_libraries[i].ref_count > 1) { g_libraries[i].ref_count--; return 1; }
            for (size_t s = 0; s < g_symbol_count;) {
                if (g_symbols[s].library_handle == handle) {
                    free(g_symbols[s].name);
                    if (s + 1 < g_symbol_count) memmove(&g_symbols[s], &g_symbols[s + 1], (g_symbol_count - s - 1) * sizeof(g_symbols[0]));
                    g_symbol_count--;
                } else {
                    s++;
                }
            }
            char *path = g_libraries[i].path;
            if (!ffi_platform_unload(handle)) return 0;
            free(path);
            if (i + 1 < g_library_count) memmove(&g_libraries[i], &g_libraries[i + 1], (g_library_count - i - 1) * sizeof(g_libraries[0]));
            g_library_count--;
            return 1;
        }
    }
    ffi_set_error("unload_library failed", "handle is not registered");
    return 0;
}

static void *ffi_symbol_cache_find(void *library_handle, const char *function_name) {
    for (size_t i = 0; i < g_symbol_count; i++) {
        if (g_symbols[i].library_handle == library_handle && g_symbols[i].name && strcmp(g_symbols[i].name, function_name) == 0) return g_symbols[i].function_ptr;
    }
    return NULL;
}

static int ffi_symbol_cache_add(void *library_handle, const char *function_name, void *function_ptr) {
    if (!function_ptr) return 1;
    if (!ffi_ensure_symbol_capacity(g_symbol_count + 1)) return 0;
    char *owned_name = ffi_strdup(function_name);
    if (!owned_name) { ffi_set_error("symbol cache failed", "out of memory"); return 0; }
    g_symbols[g_symbol_count].name = owned_name;
    g_symbols[g_symbol_count].library_handle = library_handle;
    g_symbols[g_symbol_count].function_ptr = function_ptr;
    g_symbol_count++;
    return 1;
}

void *scml_ffi_get_symbol(void *library_handle, const char *function_name) {
    if (!library_handle || !function_name || !function_name[0]) { ffi_set_error("get_symbol failed", "null handle or empty function name"); return NULL; }
    void *cached = ffi_symbol_cache_find(library_handle, function_name);
    if (cached) return cached;
    ffi_clear_error();
    void *p = ffi_platform_symbol(library_handle, function_name);
    if (p && !ffi_symbol_cache_add(library_handle, function_name, p)) return NULL;
    return p;
}

void *scml_ffi_find_symbol(const char *function_name) {
    if (!function_name || !function_name[0]) { ffi_set_error("find_symbol failed", "empty function name"); return NULL; }
    for (size_t i = 0; i < g_symbol_count; i++) {
        if (g_symbols[i].name && strcmp(g_symbols[i].name, function_name) == 0) return g_symbols[i].function_ptr;
    }
    for (size_t i = 0; i < g_library_count; i++) {
        void *p = scml_ffi_get_symbol(g_libraries[i].handle, function_name);
        if (p) return p;
    }
    ffi_set_error("find_symbol failed", function_name);
    return NULL;
}

static int ffi_token_equals(const char *start, size_t len, const char *word) {
    size_t wn = strlen(word);
    if (len != wn) return 0;
    for (size_t i = 0; i < len; i++) if ((char)tolower((unsigned char)start[i]) != word[i]) return 0;
    return 1;
}

ScmlFFIType scml_ffi_parse_type(const char *name, ScmlFFIType fallback) {
    if (!name) return fallback;
    while (*name && isspace((unsigned char)*name)) name++;
    const char *end = name + strlen(name);
    while (end > name && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - name);
    if (ffi_token_equals(name, n, "int") || ffi_token_equals(name, n, "i32") || ffi_token_equals(name, n, "int32")) return SCML_FFI_TYPE_INT32;
    if (ffi_token_equals(name, n, "i64") || ffi_token_equals(name, n, "int64") || ffi_token_equals(name, n, "long")) return SCML_FFI_TYPE_INT64;
    if (ffi_token_equals(name, n, "float") || ffi_token_equals(name, n, "f32")) return SCML_FFI_TYPE_FLOAT;
    if (ffi_token_equals(name, n, "double") || ffi_token_equals(name, n, "f64")) return SCML_FFI_TYPE_DOUBLE;
    if (ffi_token_equals(name, n, "pointer") || ffi_token_equals(name, n, "ptr") || ffi_token_equals(name, n, "void*")) return SCML_FFI_TYPE_POINTER;
    if (ffi_token_equals(name, n, "string") || ffi_token_equals(name, n, "char*") || ffi_token_equals(name, n, "cstring")) return SCML_FFI_TYPE_STRING;
    if (ffi_token_equals(name, n, "void")) return SCML_FFI_TYPE_VOID;
    return fallback;
}

ScmlFFIReturnType scml_ffi_parse_return_type(const char *name, ScmlFFIReturnType fallback) {
    return scml_ffi_parse_type(name, fallback);
}

size_t scml_ffi_parse_arg_types(const char *spec, ScmlFFIType *out_types, size_t max_types) {
    if (!spec || !out_types || max_types == 0) return 0;
    size_t count = 0;
    const char *p = spec;
    while (*p && count < max_types) {
        while (*p == ',' || isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',') p++;
        char token[32];
        size_t n = (size_t)(p - start);
        while (n > 0 && isspace((unsigned char)start[n - 1])) n--;
        if (n >= sizeof(token)) n = sizeof(token) - 1;
        memcpy(token, start, n);
        token[n] = '\0';
        out_types[count++] = scml_ffi_parse_type(token, SCML_FFI_TYPE_INT32);
    }
    return count;
}

static ScmlFFISignatureEntry *ffi_signature_entry_find(const char *function_name) {
    if (!function_name) return NULL;
    for (size_t i = 0; i < g_signature_count; i++) {
        if (g_signatures[i].name && strcmp(g_signatures[i].name, function_name) == 0) return &g_signatures[i];
    }
    return NULL;
}

int scml_ffi_declare_function(const char *function_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count) {
    if (!function_name || !function_name[0]) { ffi_set_error("declare_function failed", "empty function name"); return 0; }
    if (arg_count > 0 && !arg_types) { ffi_set_error("declare_function failed", "missing argument types"); return 0; }
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    if (!entry) {
        if (!ffi_ensure_signature_capacity(g_signature_count + 1)) return 0;
        char *owned_name = ffi_strdup(function_name);
        if (!owned_name) { ffi_set_error("declare_function failed", "out of memory"); return 0; }
        entry = &g_signatures[g_signature_count++];
        entry->name = owned_name;
        entry->arg_types = NULL;
        entry->signature.arg_types = NULL;
        entry->signature.arg_count = 0;
        entry->signature.return_type = SCML_FFI_TYPE_INT32;
    }
    ScmlFFIType *owned_args = NULL;
    if (arg_count > 0) {
        owned_args = (ScmlFFIType *)malloc(arg_count * sizeof(*owned_args));
        if (!owned_args) { ffi_set_error("declare_function failed", "out of memory"); return 0; }
        memcpy(owned_args, arg_types, arg_count * sizeof(*owned_args));
    }
    free(entry->arg_types);
    entry->arg_types = owned_args;
    entry->signature.return_type = return_type;
    entry->signature.arg_types = entry->arg_types;
    entry->signature.arg_count = arg_count;
    return 1;
}

int scml_ffi_declare_function_text(const char *function_name, const char *return_type, const char *arg_types) {
    ScmlFFIType parsed_return = scml_ffi_parse_return_type(return_type, SCML_FFI_TYPE_INT32);
    ScmlFFIType *parsed_args = NULL;
    size_t count = 0;
    if (arg_types && arg_types[0]) {
        count = 1;
        for (const char *p = arg_types; *p; p++) if (*p == ',') count++;
        parsed_args = (ScmlFFIType *)calloc(count, sizeof(*parsed_args));
        if (!parsed_args) { ffi_set_error("declare_function failed", "out of memory"); return 0; }
        count = scml_ffi_parse_arg_types(arg_types, parsed_args, count);
    }
    int ok = scml_ffi_declare_function(function_name, parsed_return, parsed_args, count);
    free(parsed_args);
    return ok;
}

int scml_ffi_undeclare_function(const char *function_name) {
    if (!function_name || !function_name[0]) { ffi_set_error("undeclare_function failed", "empty function name"); return 0; }
    for (size_t i = 0; i < g_signature_count; i++) {
        if (strcmp(g_signatures[i].name, function_name) == 0) {
            free(g_signatures[i].name);
            free(g_signatures[i].arg_types);
            if (i + 1 < g_signature_count) memmove(&g_signatures[i], &g_signatures[i + 1], (g_signature_count - i - 1) * sizeof(g_signatures[0]));
            g_signature_count--;
            return 1;
        }
    }
    ffi_set_error("undeclare_function failed", "function is not declared");
    return 0;
}

const ScmlFFISignature *scml_ffi_get_declared_signature(const char *function_name) {
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    return entry ? &entry->signature : NULL;
}

#if defined(SCML_USE_LIBFFI)
static ScmlFFIType ffi_default_arg_type(const ScmlValue *v) {
    if (v->type == SCML_VAL_FLOAT) return SCML_FFI_TYPE_FLOAT;
    if (v->type == SCML_VAL_STRING) return SCML_FFI_TYPE_STRING;
    if (v->type == SCML_VAL_POINTER) return SCML_FFI_TYPE_POINTER;
    return SCML_FFI_TYPE_INT32;
}

#endif

static uintptr_t ffi_pointer_value(const ScmlValue *v) {
    if (v->type == SCML_VAL_POINTER) return v->pointer;
    if (v->type == SCML_VAL_STRING) return (uintptr_t)(v->string ? v->string : "");
    if (v->type == SCML_VAL_FLOAT) return (uintptr_t)(intptr_t)v->real;
    return (uintptr_t)(intptr_t)v->integer;
}

static int64_t SCML_FFI_MAYBE_UNUSED ffi_int64_value(const ScmlValue *v) {
    if (v->type == SCML_VAL_POINTER) return (int64_t)(intptr_t)v->pointer;
    if (v->type == SCML_VAL_FLOAT) return (int64_t)v->real;
    if (v->type == SCML_VAL_STRING) return v->string ? (int64_t)strtoll(v->string, NULL, 0) : 0;
    return (int64_t)v->integer;
}

static double SCML_FFI_MAYBE_UNUSED ffi_double_value(const ScmlValue *v) {
    if (v->type == SCML_VAL_FLOAT) return (double)v->real;
    if (v->type == SCML_VAL_POINTER) return (double)v->pointer;
    if (v->type == SCML_VAL_STRING) return v->string ? strtod(v->string, NULL) : 0.0;
    return (double)v->integer;
}


int scml_ffi_call_native(void *function_ptr, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret) {
    ScmlFFISignature signature;
    signature.return_type = return_type;
    signature.arg_types = NULL;
    signature.arg_count = arg_count;
    return scml_ffi_call_native_ex(function_ptr, args, &signature, ret);
}

int scml_ffi_call_native_ex(void *function_ptr, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret) {
    if (!function_ptr) { ffi_set_error("call_native failed", "null function pointer"); return 0; }
    if (!signature) { ffi_set_error("call_native failed", "missing signature"); return 0; }
    size_t arg_count = signature->arg_count;
    if (arg_count > 0 && !args) { ffi_set_error("call_native failed", "missing arguments"); return 0; }
#if defined(SCML_USE_LIBFFI)
    ffi_cif cif;
    ffi_type **arg_types = NULL;
    void **arg_values = NULL;
    int32_t *int32_args = NULL;
    int64_t *int64_args = NULL;
    float *float_args = NULL;
    double *double_args = NULL;
    void **ptr_args = NULL;
    int ok = 0;

    if (arg_count > 0) {
        arg_types = (ffi_type **)calloc(arg_count, sizeof(*arg_types));
        arg_values = (void **)calloc(arg_count, sizeof(*arg_values));
        int32_args = (int32_t *)calloc(arg_count, sizeof(*int32_args));
        int64_args = (int64_t *)calloc(arg_count, sizeof(*int64_args));
        float_args = (float *)calloc(arg_count, sizeof(*float_args));
        double_args = (double *)calloc(arg_count, sizeof(*double_args));
        ptr_args = (void **)calloc(arg_count, sizeof(*ptr_args));
        if (!arg_types || !arg_values || !int32_args || !int64_args || !float_args || !double_args || !ptr_args) {
            ffi_set_error("call_native failed", "out of memory preparing arguments");
            goto cleanup;
        }
    }

    for (size_t i = 0; i < arg_count; i++) {
        ScmlFFIType type = signature->arg_types ? signature->arg_types[i] : ffi_default_arg_type(&args[i]);
        switch (type) {
        case SCML_FFI_TYPE_FLOAT:
            float_args[i] = (float)ffi_double_value(&args[i]);
            arg_types[i] = &ffi_type_float;
            arg_values[i] = &float_args[i];
            break;
        case SCML_FFI_TYPE_DOUBLE:
            double_args[i] = ffi_double_value(&args[i]);
            arg_types[i] = &ffi_type_double;
            arg_values[i] = &double_args[i];
            break;
        case SCML_FFI_TYPE_INT64:
            int64_args[i] = ffi_int64_value(&args[i]);
            arg_types[i] = &ffi_type_sint64;
            arg_values[i] = &int64_args[i];
            break;
        case SCML_FFI_TYPE_POINTER:
        case SCML_FFI_TYPE_STRING:
            ptr_args[i] = (void *)ffi_pointer_value(&args[i]);
            arg_types[i] = &ffi_type_pointer;
            arg_values[i] = &ptr_args[i];
            break;
        case SCML_FFI_TYPE_VOID:
        case SCML_FFI_TYPE_INT32:
        default:
            int32_args[i] = (int32_t)ffi_int64_value(&args[i]);
            arg_types[i] = &ffi_type_sint32;
            arg_values[i] = &int32_args[i];
            break;
        }
    }

    ffi_type *rtype = &ffi_type_sint32;
    if (signature->return_type == SCML_FFI_TYPE_FLOAT) rtype = &ffi_type_float;
    else if (signature->return_type == SCML_FFI_TYPE_DOUBLE) rtype = &ffi_type_double;
    else if (signature->return_type == SCML_FFI_TYPE_INT64) rtype = &ffi_type_sint64;
    else if (signature->return_type == SCML_FFI_TYPE_POINTER || signature->return_type == SCML_FFI_TYPE_STRING) rtype = &ffi_type_pointer;
    else if (signature->return_type == SCML_FFI_TYPE_VOID) rtype = &ffi_type_void;

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)arg_count, rtype, arg_types) != FFI_OK) {
        ffi_set_error("call_native failed", "ffi_prep_cif failed");
        goto cleanup;
    }

    if (signature->return_type == SCML_FFI_TYPE_FLOAT) {
        float out = 0.0f;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_float(out);
    } else if (signature->return_type == SCML_FFI_TYPE_DOUBLE) {
        double out = 0.0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_float((float)out);
    } else if (signature->return_type == SCML_FFI_TYPE_POINTER || signature->return_type == SCML_FFI_TYPE_STRING) {
        void *out = NULL;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_pointer((uintptr_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_INT64) {
        int64_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_pointer((uintptr_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_VOID) {
        ffi_call(&cif, FFI_FN(function_ptr), NULL, arg_values);
        if (ret) *ret = scml_value_int(0);
    } else {
        int32_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_int(out);
    }
    ok = 1;

cleanup:
    free(arg_types);
    free(arg_values);
    free(int32_args);
    free(int64_args);
    free(float_args);
    free(double_args);
    free(ptr_args);
    return ok;
#else
    if (arg_count > SCML_FFI_FALLBACK_MAX_ARGS) { ffi_set_error("call_native failed", "too many arguments without libffi"); return 0; }
    if (signature->arg_types) {
        for (size_t i = 0; i < arg_count; i++) {
            if (signature->arg_types[i] == SCML_FFI_TYPE_FLOAT || signature->arg_types[i] == SCML_FFI_TYPE_DOUBLE || signature->arg_types[i] == SCML_FFI_TYPE_INT64) {
                ffi_set_error("call_native failed", "float/double/int64 signatures require libffi");
                return 0;
            }
        }
    }
    typedef uintptr_t (*Fn0)(void);
    typedef uintptr_t (*Fn1)(uintptr_t);
    typedef uintptr_t (*Fn2)(uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn3)(uintptr_t, uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn4)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn5)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn6)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn7)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    typedef uintptr_t (*Fn8)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    uintptr_t a[SCML_FFI_FALLBACK_MAX_ARGS] = {0};
    for (size_t i = 0; i < arg_count; i++) a[i] = ffi_pointer_value(&args[i]);
    union { void *p; Fn0 f0; Fn1 f1; Fn2 f2; Fn3 f3; Fn4 f4; Fn5 f5; Fn6 f6; Fn7 f7; Fn8 f8; } fn;
    fn.p = function_ptr;
    uintptr_t out = 0;
    switch (arg_count) {
    case 0: out = fn.f0(); break;
    case 1: out = fn.f1(a[0]); break;
    case 2: out = fn.f2(a[0], a[1]); break;
    case 3: out = fn.f3(a[0], a[1], a[2]); break;
    case 4: out = fn.f4(a[0], a[1], a[2], a[3]); break;
    case 5: out = fn.f5(a[0], a[1], a[2], a[3], a[4]); break;
    case 6: out = fn.f6(a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 7: out = fn.f7(a[0], a[1], a[2], a[3], a[4], a[5], a[6]); break;
    case 8: out = fn.f8(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); break;
    default: return 0;
    }
    if (ret) {
        if (signature->return_type == SCML_FFI_TYPE_POINTER || signature->return_type == SCML_FFI_TYPE_STRING || signature->return_type == SCML_FFI_TYPE_INT64) *ret = scml_value_pointer(out);
        else *ret = scml_value_int((int32_t)out);
    }
    return signature->return_type != SCML_FFI_TYPE_FLOAT && signature->return_type != SCML_FFI_TYPE_DOUBLE;
#endif
}

int scml_ffi_call_native_by_name(const char *function_name, const ScmlValue *args, size_t arg_count, ScmlFFIReturnType return_type, ScmlValue *ret) {
    ScmlFFISignature signature;
    signature.return_type = return_type;
    signature.arg_types = NULL;
    signature.arg_count = arg_count;
    return scml_ffi_call_native_by_name_ex(function_name, args, &signature, ret);
}

int scml_ffi_call_native_by_name_ex(const char *function_name, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret) {
    void *symbol = scml_ffi_find_symbol(function_name);
    if (!symbol) return 0;
    const ScmlFFISignature *declared = scml_ffi_get_declared_signature(function_name);
    if (declared && (!signature || !signature->arg_types)) signature = declared;
    return scml_ffi_call_native_ex(symbol, args, signature, ret);
}

static ScmlFFIMemoryBlock *ffi_memory_find(void *ptr) {
    if (!ptr) return NULL;
    for (size_t i = 0; i < g_memory_block_count; i++) if (g_memory_blocks[i].ptr == ptr) return &g_memory_blocks[i];
    return NULL;
}

static ScmlFFIMemoryBlock *ffi_memory_find_containing(const void *ptr, size_t access_size) {
    if (!ptr) return NULL;
    uintptr_t target = (uintptr_t)ptr;
    for (size_t i = 0; i < g_memory_block_count; i++) {
        uintptr_t start = (uintptr_t)g_memory_blocks[i].ptr;
        uintptr_t end = start + g_memory_blocks[i].size;
        if (target >= start && target <= end && access_size <= (size_t)(end - target)) return &g_memory_blocks[i];
    }
    return NULL;
}

static int ffi_memory_check_access(const void *ptr, size_t access_size) {
    if (!ptr) { ffi_set_error("memory access failed", "null pointer"); return 0; }
    ScmlFFIMemoryBlock *block = ffi_memory_find_containing(ptr, access_size);
    if (block || g_memory_block_count == 0) return 1;
    for (size_t i = 0; i < g_memory_block_count; i++) {
        uintptr_t start = (uintptr_t)g_memory_blocks[i].ptr;
        uintptr_t end = start + g_memory_blocks[i].size;
        uintptr_t target = (uintptr_t)ptr;
        if (target >= start && target <= end) { ffi_set_error("memory access failed", "tracked allocation bounds exceeded"); return 0; }
    }
    return 1;
}

static size_t ffi_type_size(ScmlFFIType type) {
    switch (type) {
    case SCML_FFI_TYPE_FLOAT: return sizeof(float);
    case SCML_FFI_TYPE_DOUBLE: return sizeof(double);
    case SCML_FFI_TYPE_INT64: return sizeof(int64_t);
    case SCML_FFI_TYPE_POINTER:
    case SCML_FFI_TYPE_STRING: return sizeof(void *);
    case SCML_FFI_TYPE_VOID: return 0;
    case SCML_FFI_TYPE_INT32:
    default: return sizeof(int32_t);
    }
}

void *scml_ffi_alloc(size_t size) {
    if (size == 0) { ffi_set_error("alloc failed", "size must be greater than zero"); return NULL; }
    if (!ffi_ensure_memory_block_capacity(g_memory_block_count + 1)) return NULL;
    void *ptr = calloc(1, size);
    if (!ptr) { ffi_set_error("alloc failed", "out of memory"); return NULL; }
    g_memory_blocks[g_memory_block_count].ptr = ptr;
    g_memory_blocks[g_memory_block_count].size = size;
    g_memory_block_count++;
    return ptr;
}

void *scml_ffi_alloc_cstring(const char *text) {
    size_t size = strlen(text ? text : "") + 1;
    char *ptr = (char *)scml_ffi_alloc(size);
    if (!ptr) return NULL;
    memcpy(ptr, text ? text : "", size);
    return ptr;
}

char *scml_ffi_read_cstring(const void *ptr) {
    if (!ptr) { ffi_set_error("read_cstring failed", "null pointer"); return NULL; }
    size_t limit = 0;
    ScmlFFIMemoryBlock *block = ffi_memory_find_containing(ptr, 1);
    if (block) {
        uintptr_t start = (uintptr_t)block->ptr;
        uintptr_t target = (uintptr_t)ptr;
        limit = block->size - (size_t)(target - start);
    } else {
        limit = 4096;
    }
    const char *s = (const char *)ptr;
    size_t n = 0;
    while (n < limit && s[n] != '\0') n++;
    if (n == limit) { ffi_set_error("read_cstring failed", "unterminated string before safety limit"); return NULL; }
    char *out = (char *)malloc(n + 1);
    if (!out) { ffi_set_error("read_cstring failed", "out of memory"); return NULL; }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

int scml_ffi_free_memory(void *ptr) {
    if (!ptr) { ffi_set_error("free failed", "null pointer"); return 0; }
    for (size_t i = 0; i < g_memory_block_count; i++) {
        if (g_memory_blocks[i].ptr == ptr) {
            free(ptr);
            if (i + 1 < g_memory_block_count) memmove(&g_memory_blocks[i], &g_memory_blocks[i + 1], (g_memory_block_count - i - 1) * sizeof(g_memory_blocks[0]));
            g_memory_block_count--;
            return 1;
        }
    }
    ffi_set_error("free failed", "pointer is not owned by FFI allocator");
    return 0;
}

void *scml_ffi_ptr_add(void *ptr, intptr_t offset) {
    if (!ptr) { ffi_set_error("ptr_add failed", "null pointer"); return NULL; }
    return (void *)((unsigned char *)ptr + offset);
}

int scml_ffi_memory_read(void *base, size_t offset, ScmlFFIType type, ScmlValue *ret) {
    if (!ret) { ffi_set_error("memory read failed", "missing output"); return 0; }
    unsigned char *addr = (unsigned char *)base + offset;
    size_t size = ffi_type_size(type);
    if (!ffi_memory_check_access(addr, size)) return 0;
    switch (type) {
    case SCML_FFI_TYPE_FLOAT: { float v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_float(v); return 1; }
    case SCML_FFI_TYPE_DOUBLE: { double v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_float((float)v); return 1; }
    case SCML_FFI_TYPE_INT64: { int64_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_pointer((uintptr_t)v); return 1; }
    case SCML_FFI_TYPE_POINTER:
    case SCML_FFI_TYPE_STRING: { void *v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_pointer((uintptr_t)v); return 1; }
    case SCML_FFI_TYPE_INT32:
    default: { int32_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_int(v); return 1; }
    }
}

int scml_ffi_memory_write(void *base, size_t offset, ScmlFFIType type, const ScmlValue *value) {
    if (!value) { ffi_set_error("memory write failed", "missing value"); return 0; }
    unsigned char *addr = (unsigned char *)base + offset;
    size_t size = ffi_type_size(type);
    if (!ffi_memory_check_access(addr, size)) return 0;
    switch (type) {
    case SCML_FFI_TYPE_FLOAT: { float v = (float)ffi_double_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_DOUBLE: { double v = ffi_double_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT64: { int64_t v = ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_POINTER: { void *v = (void *)ffi_pointer_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_STRING: { const char *v = value->type == SCML_VAL_STRING ? (value->string ? value->string : "") : (const char *)ffi_pointer_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT32:
    default: { int32_t v = (int32_t)ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    }
}

int scml_ffi_memory_copy(void *dst, const void *src, size_t size) {
    if (!ffi_memory_check_access(dst, size) || !ffi_memory_check_access(src, size)) return 0;
    memmove(dst, src, size);
    return 1;
}

int scml_ffi_memory_set(void *dst, int value, size_t size) {
    if (!ffi_memory_check_access(dst, size)) return 0;
    memset(dst, value, size);
    return 1;
}

size_t scml_ffi_memory_block_size(void *ptr) {
    ScmlFFIMemoryBlock *block = ffi_memory_find(ptr);
    return block ? block->size : 0;
}

void scml_ffi_get_stats(ScmlFFIStats *out_stats) {
    if (!out_stats) return;
    out_stats->library_count = g_library_count;
    out_stats->library_capacity = g_library_capacity;
    out_stats->symbol_count = g_symbol_count;
    out_stats->symbol_capacity = g_symbol_capacity;
    out_stats->signature_count = g_signature_count;
    out_stats->signature_capacity = g_signature_capacity;
    out_stats->search_path_count = g_search_path_count;
    out_stats->search_path_capacity = g_search_path_capacity;
    out_stats->memory_block_count = g_memory_block_count;
    out_stats->memory_block_capacity = g_memory_block_capacity;
}

void scml_ffi_shutdown(void) {
    for (size_t i = 0; i < g_symbol_count; i++) free(g_symbols[i].name);
    free(g_symbols);
    g_symbols = NULL;
    g_symbol_count = 0;
    g_symbol_capacity = 0;
    for (size_t i = 0; i < g_signature_count; i++) { free(g_signatures[i].name); free(g_signatures[i].arg_types); }
    free(g_signatures);
    g_signatures = NULL;
    g_signature_count = 0;
    g_signature_capacity = 0;
    for (size_t i = 0; i < g_search_path_count; i++) free(g_search_paths[i].path);
    free(g_search_paths);
    g_search_paths = NULL;
    g_search_path_count = 0;
    g_search_path_capacity = 0;
    for (size_t i = 0; i < g_memory_block_count; i++) free(g_memory_blocks[i].ptr);
    free(g_memory_blocks);
    g_memory_blocks = NULL;
    g_memory_block_count = 0;
    g_memory_block_capacity = 0;
    while (g_library_count > 0) {
        size_t i = g_library_count - 1;
        void *handle = g_libraries[i].handle;
        char *path = g_libraries[i].path;
        ffi_platform_unload(handle);
        free(path);
        g_library_count--;
    }
    free(g_libraries);
    g_libraries = NULL;
    g_library_capacity = 0;
    ffi_clear_error();
}
