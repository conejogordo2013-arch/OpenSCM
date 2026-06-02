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
    char *symbol_name;
    void *library_handle;
    void *function_ptr;
    ScmlFFISignature signature;
    ScmlFFIType *arg_types;
    ScmlFFIAbi abi;
} ScmlFFISignatureEntry;

typedef struct ScmlFFISearchPath {
    char *path;
} ScmlFFISearchPath;

typedef struct ScmlFFIMemoryBlock {
    void *ptr;
    size_t size;
    int owned;
} ScmlFFIMemoryBlock;

typedef struct ScmlFFIStructField {
    char *name;
    ScmlFFIType type;
    size_t offset;
    size_t size;
    size_t alignment;
    size_t element_size;
    size_t element_count;
} ScmlFFIStructField;

typedef struct ScmlFFIStructDef {
    char *name;
    ScmlFFIStructField *fields;
    size_t field_count;
    size_t field_capacity;
    size_t size;
    size_t alignment;
    int finished;
    int is_union;
} ScmlFFIStructDef;

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
static ScmlFFIStructDef *g_structs = NULL;
static size_t g_struct_count = 0;
static size_t g_struct_capacity = 0;
static size_t g_struct_field_count = 0;
static size_t g_struct_field_capacity = 0;
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

void scml_ffi_clear_error(void) { ffi_clear_error(); }


static char *ffi_trim(char *s) {
    while (s && *s && isspace((unsigned char)*s)) s++;
    if (!s) return s;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

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

static int ffi_ensure_struct_capacity(size_t min_capacity) {
    return ffi_grow_array((void **)&g_structs, sizeof(g_structs[0]), &g_struct_capacity, min_capacity, SCML_FFI_INITIAL_STRUCTS);
}

static int ffi_ensure_struct_field_capacity(ScmlFFIStructDef *def, size_t min_capacity) {
    int ok = ffi_grow_array((void **)&def->fields, sizeof(def->fields[0]), &def->field_capacity, min_capacity, SCML_FFI_INITIAL_STRUCT_FIELDS);
    if (ok && g_struct_field_capacity < g_struct_field_count + (def->field_capacity - def->field_count)) g_struct_field_capacity = g_struct_field_count + (def->field_capacity - def->field_count);
    return ok;
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
    if (ffi_token_equals(name, n, "bool") || ffi_token_equals(name, n, "boolean")) return SCML_FFI_TYPE_BOOL;
    if (ffi_token_equals(name, n, "i8") || ffi_token_equals(name, n, "int8") || ffi_token_equals(name, n, "char")) return SCML_FFI_TYPE_INT8;
    if (ffi_token_equals(name, n, "u8") || ffi_token_equals(name, n, "uint8") || ffi_token_equals(name, n, "byte") || ffi_token_equals(name, n, "uchar")) return SCML_FFI_TYPE_UINT8;
    if (ffi_token_equals(name, n, "i16") || ffi_token_equals(name, n, "int16") || ffi_token_equals(name, n, "short")) return SCML_FFI_TYPE_INT16;
    if (ffi_token_equals(name, n, "u16") || ffi_token_equals(name, n, "uint16") || ffi_token_equals(name, n, "ushort")) return SCML_FFI_TYPE_UINT16;
    if (ffi_token_equals(name, n, "int") || ffi_token_equals(name, n, "i32") || ffi_token_equals(name, n, "int32")) return SCML_FFI_TYPE_INT32;
    if (ffi_token_equals(name, n, "u32") || ffi_token_equals(name, n, "uint") || ffi_token_equals(name, n, "uint32")) return SCML_FFI_TYPE_UINT32;
    if (ffi_token_equals(name, n, "i64") || ffi_token_equals(name, n, "int64") || ffi_token_equals(name, n, "long") || ffi_token_equals(name, n, "longlong")) return SCML_FFI_TYPE_INT64;
    if (ffi_token_equals(name, n, "u64") || ffi_token_equals(name, n, "uint64") || ffi_token_equals(name, n, "ulong") || ffi_token_equals(name, n, "ulonglong")) return SCML_FFI_TYPE_UINT64;
    if (ffi_token_equals(name, n, "size") || ffi_token_equals(name, n, "size_t") || ffi_token_equals(name, n, "usize")) return SCML_FFI_TYPE_SIZE;
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

int scml_ffi_abi_supported(ScmlFFIAbi abi) {
    switch (abi) {
    case SCML_FFI_ABI_DEFAULT:
    case SCML_FFI_ABI_CDECL:
        return 1;
    case SCML_FFI_ABI_STDCALL:
#if defined(SCML_USE_LIBFFI) && (defined(_WIN64) || (defined(_WIN32) && defined(FFI_STDCALL)))
        return 1;
#else
        return 0;
#endif
    case SCML_FFI_ABI_FASTCALL:
    case SCML_FFI_ABI_THISCALL:
#if defined(SCML_USE_LIBFFI) && defined(_WIN64)
        return 1;
#else
        return 0;
#endif
    default:
        return 0;
    }
}

char *scml_ffi_capabilities(void) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "libffi=%d fallback_max_args=%d abi={default:%d,cdecl:%d,stdcall:%d,fastcall:%d,thiscall:%d} features={call_ptr:1,call_name:1,bind_alias:1,vtable:1,peek_poke_ptr:1,utf16:1,union:1,struct_arrays:1}",
#if defined(SCML_USE_LIBFFI)
             1,
#else
             0,
#endif
             SCML_FFI_FALLBACK_MAX_ARGS,
             scml_ffi_abi_supported(SCML_FFI_ABI_DEFAULT),
             scml_ffi_abi_supported(SCML_FFI_ABI_CDECL),
             scml_ffi_abi_supported(SCML_FFI_ABI_STDCALL),
             scml_ffi_abi_supported(SCML_FFI_ABI_FASTCALL),
             scml_ffi_abi_supported(SCML_FFI_ABI_THISCALL));
    return ffi_strdup(buf);
}

ScmlFFIAbi scml_ffi_parse_abi(const char *name, ScmlFFIAbi fallback) {
    if (!name) return fallback;
    while (*name && isspace((unsigned char)*name)) name++;
    const char *end = name + strlen(name);
    while (end > name && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - name);
    if (ffi_token_equals(name, n, "default") || ffi_token_equals(name, n, "system")) return SCML_FFI_ABI_DEFAULT;
    if (ffi_token_equals(name, n, "cdecl") || ffi_token_equals(name, n, "c")) return SCML_FFI_ABI_CDECL;
    if (ffi_token_equals(name, n, "stdcall") || ffi_token_equals(name, n, "winapi")) return SCML_FFI_ABI_STDCALL;
    if (ffi_token_equals(name, n, "fastcall")) return SCML_FFI_ABI_FASTCALL;
    if (ffi_token_equals(name, n, "thiscall")) return SCML_FFI_ABI_THISCALL;
    return fallback;
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

int scml_ffi_declare_function_abi(const char *function_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count, ScmlFFIAbi abi) {
    if (!function_name || !function_name[0]) { ffi_set_error("declare_function failed", "empty function name"); return 0; }
    if (!scml_ffi_abi_supported(abi)) { ffi_set_error("declare_function failed", "ABI is not supported"); return 0; }
    if (arg_count > 0 && !arg_types) { ffi_set_error("declare_function failed", "missing argument types"); return 0; }
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    if (!entry) {
        if (!ffi_ensure_signature_capacity(g_signature_count + 1)) return 0;
        char *owned_name = ffi_strdup(function_name);
        if (!owned_name) { ffi_set_error("declare_function failed", "out of memory"); return 0; }
        entry = &g_signatures[g_signature_count++];
        entry->name = owned_name;
        entry->symbol_name = NULL;
        entry->library_handle = NULL;
        entry->function_ptr = NULL;
        entry->arg_types = NULL;
        entry->signature.arg_types = NULL;
        entry->signature.arg_count = 0;
        entry->signature.return_type = SCML_FFI_TYPE_INT32;
        entry->abi = SCML_FFI_ABI_DEFAULT;
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
    entry->abi = abi;
    return 1;
}

int scml_ffi_declare_function(const char *function_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count) {
    return scml_ffi_declare_function_abi(function_name, return_type, arg_types, arg_count, SCML_FFI_ABI_DEFAULT);
}

int scml_ffi_declare_function_text_abi(const char *function_name, const char *return_type, const char *arg_types, const char *abi_name) {
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
    ScmlFFIAbi abi = scml_ffi_parse_abi(abi_name, SCML_FFI_ABI_DEFAULT);
    int ok = scml_ffi_declare_function_abi(function_name, parsed_return, parsed_args, count, abi);
    free(parsed_args);
    return ok;
}

int scml_ffi_declare_function_text(const char *function_name, const char *return_type, const char *arg_types) {
    return scml_ffi_declare_function_text_abi(function_name, return_type, arg_types, "default");
}

int scml_ffi_bind_function(const char *alias, void *library_handle, const char *symbol_name, ScmlFFIReturnType return_type, const ScmlFFIType *arg_types, size_t arg_count, ScmlFFIAbi abi) {
    const char *lookup = (symbol_name && symbol_name[0]) ? symbol_name : alias;
    if (!alias || !alias[0] || !lookup || !lookup[0]) { ffi_set_error("bind_function failed", "empty alias or symbol name"); return 0; }
    void *symbol = library_handle ? scml_ffi_get_symbol(library_handle, lookup) : scml_ffi_find_symbol(lookup);
    if (!symbol) return 0;
    if (!scml_ffi_declare_function_abi(alias, return_type, arg_types, arg_count, abi)) return 0;
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(alias);
    if (!entry) { ffi_set_error("bind_function failed", "signature registry failed"); return 0; }
    char *owned_symbol = ffi_strdup(lookup);
    if (!owned_symbol) { ffi_set_error("bind_function failed", "out of memory"); return 0; }
    free(entry->symbol_name);
    entry->symbol_name = owned_symbol;
    entry->library_handle = library_handle;
    entry->function_ptr = symbol;
    return 1;
}

int scml_ffi_bind_function_text(const char *alias, void *library_handle, const char *symbol_name, const char *return_type, const char *arg_types, const char *abi_name) {
    ScmlFFIType parsed_return = scml_ffi_parse_return_type(return_type, SCML_FFI_TYPE_INT32);
    ScmlFFIType *parsed_args = NULL;
    size_t count = 0;
    if (arg_types && arg_types[0]) {
        count = 1;
        for (const char *p = arg_types; *p; p++) if (*p == ',') count++;
        parsed_args = (ScmlFFIType *)calloc(count, sizeof(*parsed_args));
        if (!parsed_args) { ffi_set_error("bind_function failed", "out of memory"); return 0; }
        count = scml_ffi_parse_arg_types(arg_types, parsed_args, count);
    }
    ScmlFFIAbi abi = scml_ffi_parse_abi(abi_name, SCML_FFI_ABI_DEFAULT);
    int ok = scml_ffi_bind_function(alias, library_handle, symbol_name, parsed_return, parsed_args, count, abi);
    free(parsed_args);
    return ok;
}

int scml_ffi_undeclare_function(const char *function_name) {
    if (!function_name || !function_name[0]) { ffi_set_error("undeclare_function failed", "empty function name"); return 0; }
    for (size_t i = 0; i < g_signature_count; i++) {
        if (strcmp(g_signatures[i].name, function_name) == 0) {
            free(g_signatures[i].name);
            free(g_signatures[i].symbol_name);
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

ScmlFFIAbi scml_ffi_get_declared_abi(const char *function_name) {
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    return entry ? entry->abi : SCML_FFI_ABI_DEFAULT;
}

void *scml_ffi_get_bound_symbol(const char *function_name) {
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    return entry ? entry->function_ptr : NULL;
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

static uint64_t SCML_FFI_MAYBE_UNUSED ffi_uint64_value(const ScmlValue *v) {
    if (v->type == SCML_VAL_POINTER) return (uint64_t)v->pointer;
    if (v->type == SCML_VAL_FLOAT) return (uint64_t)v->real;
    if (v->type == SCML_VAL_STRING) return v->string ? (uint64_t)strtoull(v->string, NULL, 0) : 0;
    return (uint64_t)(uint32_t)v->integer;
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
    return scml_ffi_call_native_abi(function_ptr, args, signature, SCML_FFI_ABI_DEFAULT, ret);
}

int scml_ffi_call_native_abi(void *function_ptr, const ScmlValue *args, const ScmlFFISignature *signature, ScmlFFIAbi abi, ScmlValue *ret) {
    if (!function_ptr) { ffi_set_error("call_native failed", "null function pointer"); return 0; }
    if (!signature) { ffi_set_error("call_native failed", "missing signature"); return 0; }
    size_t arg_count = signature->arg_count;
    if (arg_count > 0 && !args) { ffi_set_error("call_native failed", "missing arguments"); return 0; }
#if defined(SCML_USE_LIBFFI)
    ffi_cif cif;
    ffi_type **arg_types = NULL;
    void **arg_values = NULL;
    int8_t *int8_args = NULL;
    uint8_t *uint8_args = NULL;
    int16_t *int16_args = NULL;
    uint16_t *uint16_args = NULL;
    int32_t *int32_args = NULL;
    uint32_t *uint32_args = NULL;
    int64_t *int64_args = NULL;
    uint64_t *uint64_args = NULL;
    float *float_args = NULL;
    double *double_args = NULL;
    void **ptr_args = NULL;
    int ok = 0;

    if (arg_count > 0) {
        arg_types = (ffi_type **)calloc(arg_count, sizeof(*arg_types));
        arg_values = (void **)calloc(arg_count, sizeof(*arg_values));
        int8_args = (int8_t *)calloc(arg_count, sizeof(*int8_args));
        uint8_args = (uint8_t *)calloc(arg_count, sizeof(*uint8_args));
        int16_args = (int16_t *)calloc(arg_count, sizeof(*int16_args));
        uint16_args = (uint16_t *)calloc(arg_count, sizeof(*uint16_args));
        int32_args = (int32_t *)calloc(arg_count, sizeof(*int32_args));
        uint32_args = (uint32_t *)calloc(arg_count, sizeof(*uint32_args));
        int64_args = (int64_t *)calloc(arg_count, sizeof(*int64_args));
        uint64_args = (uint64_t *)calloc(arg_count, sizeof(*uint64_args));
        float_args = (float *)calloc(arg_count, sizeof(*float_args));
        double_args = (double *)calloc(arg_count, sizeof(*double_args));
        ptr_args = (void **)calloc(arg_count, sizeof(*ptr_args));
        if (!arg_types || !arg_values || !int8_args || !uint8_args || !int16_args || !uint16_args || !int32_args || !uint32_args || !int64_args || !uint64_args || !float_args || !double_args || !ptr_args) {
            ffi_set_error("call_native failed", "out of memory preparing arguments");
            goto cleanup;
        }
    }

    for (size_t i = 0; i < arg_count; i++) {
        ScmlFFIType type = signature->arg_types ? signature->arg_types[i] : ffi_default_arg_type(&args[i]);
        switch (type) {
        case SCML_FFI_TYPE_BOOL:
        case SCML_FFI_TYPE_UINT8:
            uint8_args[i] = (uint8_t)ffi_uint64_value(&args[i]);
            arg_types[i] = &ffi_type_uint8;
            arg_values[i] = &uint8_args[i];
            break;
        case SCML_FFI_TYPE_INT8:
            int8_args[i] = (int8_t)ffi_int64_value(&args[i]);
            arg_types[i] = &ffi_type_sint8;
            arg_values[i] = &int8_args[i];
            break;
        case SCML_FFI_TYPE_UINT16:
            uint16_args[i] = (uint16_t)ffi_uint64_value(&args[i]);
            arg_types[i] = &ffi_type_uint16;
            arg_values[i] = &uint16_args[i];
            break;
        case SCML_FFI_TYPE_INT16:
            int16_args[i] = (int16_t)ffi_int64_value(&args[i]);
            arg_types[i] = &ffi_type_sint16;
            arg_values[i] = &int16_args[i];
            break;
        case SCML_FFI_TYPE_UINT32:
            uint32_args[i] = (uint32_t)ffi_uint64_value(&args[i]);
            arg_types[i] = &ffi_type_uint32;
            arg_values[i] = &uint32_args[i];
            break;
        case SCML_FFI_TYPE_UINT64:
        case SCML_FFI_TYPE_SIZE:
            uint64_args[i] = (uint64_t)ffi_uint64_value(&args[i]);
            arg_types[i] = sizeof(size_t) == 8 && type == SCML_FFI_TYPE_SIZE ? &ffi_type_uint64 : (type == SCML_FFI_TYPE_SIZE ? &ffi_type_uint32 : &ffi_type_uint64);
            arg_values[i] = &uint64_args[i];
            break;
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
    if (signature->return_type == SCML_FFI_TYPE_BOOL || signature->return_type == SCML_FFI_TYPE_UINT8) rtype = &ffi_type_uint8;
    else if (signature->return_type == SCML_FFI_TYPE_INT8) rtype = &ffi_type_sint8;
    else if (signature->return_type == SCML_FFI_TYPE_UINT16) rtype = &ffi_type_uint16;
    else if (signature->return_type == SCML_FFI_TYPE_INT16) rtype = &ffi_type_sint16;
    else if (signature->return_type == SCML_FFI_TYPE_UINT32) rtype = &ffi_type_uint32;
    else if (signature->return_type == SCML_FFI_TYPE_UINT64) rtype = &ffi_type_uint64;
    else if (signature->return_type == SCML_FFI_TYPE_SIZE) rtype = sizeof(size_t) == 8 ? &ffi_type_uint64 : &ffi_type_uint32;
    else if (signature->return_type == SCML_FFI_TYPE_FLOAT) rtype = &ffi_type_float;
    else if (signature->return_type == SCML_FFI_TYPE_DOUBLE) rtype = &ffi_type_double;
    else if (signature->return_type == SCML_FFI_TYPE_INT64) rtype = &ffi_type_sint64;
    else if (signature->return_type == SCML_FFI_TYPE_POINTER || signature->return_type == SCML_FFI_TYPE_STRING) rtype = &ffi_type_pointer;
    else if (signature->return_type == SCML_FFI_TYPE_VOID) rtype = &ffi_type_void;

    ffi_abi ffi_abi_value = FFI_DEFAULT_ABI;
    (void)abi;
#if defined(_WIN32) && defined(FFI_STDCALL)
    if (abi == SCML_FFI_ABI_STDCALL) ffi_abi_value = FFI_STDCALL;
#endif
#if defined(FFI_SYSV)
    if (abi == SCML_FFI_ABI_CDECL) ffi_abi_value = FFI_SYSV;
#endif
#if defined(FFI_WIN64) && defined(_WIN64)
    if (abi == SCML_FFI_ABI_CDECL || abi == SCML_FFI_ABI_STDCALL || abi == SCML_FFI_ABI_FASTCALL || abi == SCML_FFI_ABI_THISCALL) ffi_abi_value = FFI_WIN64;
#endif

    if ((abi == SCML_FFI_ABI_FASTCALL || abi == SCML_FFI_ABI_THISCALL) && ffi_abi_value == FFI_DEFAULT_ABI) {
        ffi_set_error("call_native failed", "requested ABI is not supported by this libffi build");
        goto cleanup;
    }

    if (ffi_prep_cif(&cif, ffi_abi_value, (unsigned int)arg_count, rtype, arg_types) != FFI_OK) {
        ffi_set_error("call_native failed", "ffi_prep_cif failed");
        goto cleanup;
    }

    if (signature->return_type == SCML_FFI_TYPE_BOOL || signature->return_type == SCML_FFI_TYPE_UINT8) {
        uint8_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_int((int32_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_INT8) {
        int8_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_int((int32_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_UINT16) {
        uint16_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_int((int32_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_INT16) {
        int16_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_int((int32_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_UINT32) {
        uint32_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_pointer((uintptr_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_UINT64 || signature->return_type == SCML_FFI_TYPE_SIZE) {
        uint64_t out = 0;
        ffi_call(&cif, FFI_FN(function_ptr), &out, arg_values);
        if (ret) *ret = scml_value_pointer((uintptr_t)out);
    } else if (signature->return_type == SCML_FFI_TYPE_FLOAT) {
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
    free(int8_args);
    free(uint8_args);
    free(int16_args);
    free(uint16_args);
    free(int32_args);
    free(uint32_args);
    free(int64_args);
    free(uint64_args);
    free(float_args);
    free(double_args);
    free(ptr_args);
    return ok;
#else
    if (abi != SCML_FFI_ABI_DEFAULT && abi != SCML_FFI_ABI_CDECL) { ffi_set_error("call_native failed", "non-default ABI requires libffi"); return 0; }
    if (arg_count > SCML_FFI_FALLBACK_MAX_ARGS) { ffi_set_error("call_native failed", "too many arguments without libffi"); return 0; }
    if (signature->arg_types) {
        for (size_t i = 0; i < arg_count; i++) {
            if (signature->arg_types[i] == SCML_FFI_TYPE_FLOAT || signature->arg_types[i] == SCML_FFI_TYPE_DOUBLE) {
                ffi_set_error("call_native failed", "float/double signatures require libffi");
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

int scml_ffi_call_native_by_name_abi(const char *function_name, const ScmlValue *args, const ScmlFFISignature *signature, ScmlFFIAbi abi, ScmlValue *ret) {
    if (!function_name || !function_name[0]) { ffi_set_error("call_native_by_name failed", "empty function name"); return 0; }
    ScmlFFISignatureEntry *entry = ffi_signature_entry_find(function_name);
    const char *lookup_name = entry && entry->symbol_name ? entry->symbol_name : function_name;
    void *symbol = entry && entry->function_ptr ? entry->function_ptr : scml_ffi_find_symbol(lookup_name);
    if (!symbol) return 0;
    if (entry && !entry->function_ptr) entry->function_ptr = symbol;
    const ScmlFFISignature *declared = entry ? &entry->signature : NULL;
    if (declared && (!signature || !signature->arg_types)) signature = declared;
    ScmlFFIAbi effective_abi = abi == SCML_FFI_ABI_DEFAULT && entry ? entry->abi : abi;
    return scml_ffi_call_native_abi(symbol, args, signature, effective_abi, ret);
}

int scml_ffi_call_native_by_name_ex(const char *function_name, const ScmlValue *args, const ScmlFFISignature *signature, ScmlValue *ret) {
    return scml_ffi_call_native_by_name_abi(function_name, args, signature, SCML_FFI_ABI_DEFAULT, ret);
}


static int ffi_memory_check_access(const void *ptr, size_t access_size);

void *scml_ffi_peek_pointer(const void *base, size_t index) {
    if (!base) { ffi_set_error("peek_pointer failed", "null pointer"); return NULL; }
    if (index > ((size_t)-1) / sizeof(void *)) { ffi_set_error("peek_pointer failed", "offset overflow"); return NULL; }
    const void *slot_addr = (const unsigned char *)base + index * sizeof(void *);
    if (!ffi_memory_check_access(slot_addr, sizeof(void *))) return NULL;
    void *value = NULL;
    memcpy(&value, slot_addr, sizeof(value));
    return value;
}

int scml_ffi_poke_pointer(void *base, size_t index, void *value) {
    if (!base) { ffi_set_error("poke_pointer failed", "null pointer"); return 0; }
    if (index > ((size_t)-1) / sizeof(void *)) { ffi_set_error("poke_pointer failed", "offset overflow"); return 0; }
    void *slot_addr = (unsigned char *)base + index * sizeof(void *);
    if (!ffi_memory_check_access(slot_addr, sizeof(void *))) return 0;
    memcpy(slot_addr, &value, sizeof(value));
    return 1;
}

int scml_ffi_call_vtable(void *object_ptr, size_t method_index, const ScmlValue *args, const ScmlFFISignature *signature, ScmlFFIAbi abi, int include_this, ScmlValue *ret) {
    if (!object_ptr) { ffi_set_error("call_vtable failed", "null object pointer"); return 0; }
    if (!signature) { ffi_set_error("call_vtable failed", "missing signature"); return 0; }
    void *vtable = scml_ffi_peek_pointer(object_ptr, 0);
    if (!vtable) { ffi_set_error("call_vtable failed", "null vtable pointer"); return 0; }
    void *function_ptr = scml_ffi_peek_pointer(vtable, method_index);
    if (!function_ptr) { ffi_set_error("call_vtable failed", "null method pointer"); return 0; }
    if (!include_this) return scml_ffi_call_native_abi(function_ptr, args, signature, abi, ret);

    size_t original_count = signature->arg_count;
    size_t total_count = original_count + 1;
    if (total_count == 0) { ffi_set_error("call_vtable failed", "argument count overflow"); return 0; }
    ScmlValue *with_this = (ScmlValue *)calloc(total_count, sizeof(*with_this));
    ScmlFFIType *with_types = NULL;
    if (!with_this) { ffi_set_error("call_vtable failed", "out of memory"); return 0; }
    with_this[0] = scml_value_pointer((uintptr_t)object_ptr);
    for (size_t i = 0; i < original_count; i++) with_this[i + 1] = args[i];

    ScmlFFISignature with_signature;
    with_signature.return_type = signature->return_type;
    with_signature.arg_types = NULL;
    with_signature.arg_count = total_count;
    if (signature->arg_types) {
        with_types = (ScmlFFIType *)malloc(total_count * sizeof(*with_types));
        if (!with_types) { free(with_this); ffi_set_error("call_vtable failed", "out of memory"); return 0; }
        with_types[0] = SCML_FFI_TYPE_POINTER;
        memcpy(with_types + 1, signature->arg_types, original_count * sizeof(*with_types));
        with_signature.arg_types = with_types;
    }
    int ok = scml_ffi_call_native_abi(function_ptr, with_this, &with_signature, abi, ret);
    free(with_types);
    free(with_this);
    return ok;
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
        size_t block_size = g_memory_blocks[i].size;
        if (target >= start && (size_t)(target - start) <= block_size && access_size <= block_size - (size_t)(target - start)) return &g_memory_blocks[i];
    }
    return NULL;
}

static int ffi_memory_check_access(const void *ptr, size_t access_size) {
    if (!ptr) { ffi_set_error("memory access failed", "null pointer"); return 0; }
    ScmlFFIMemoryBlock *block = ffi_memory_find_containing(ptr, access_size);
    if (block || g_memory_block_count == 0) return 1;
    for (size_t i = 0; i < g_memory_block_count; i++) {
        uintptr_t start = (uintptr_t)g_memory_blocks[i].ptr;
        size_t block_size = g_memory_blocks[i].size;
        uintptr_t target = (uintptr_t)ptr;
        if (target >= start && (size_t)(target - start) <= block_size) { ffi_set_error("memory access failed", "tracked allocation bounds exceeded"); return 0; }
    }
    return 1;
}

static size_t ffi_type_size(ScmlFFIType type) {
    switch (type) {
    case SCML_FFI_TYPE_BOOL: return sizeof(uint8_t);
    case SCML_FFI_TYPE_INT8: return sizeof(int8_t);
    case SCML_FFI_TYPE_UINT8: return sizeof(uint8_t);
    case SCML_FFI_TYPE_INT16: return sizeof(int16_t);
    case SCML_FFI_TYPE_UINT16: return sizeof(uint16_t);
    case SCML_FFI_TYPE_UINT32: return sizeof(uint32_t);
    case SCML_FFI_TYPE_UINT64: return sizeof(uint64_t);
    case SCML_FFI_TYPE_SIZE: return sizeof(size_t);
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

size_t scml_ffi_type_size(ScmlFFIType type) { return ffi_type_size(type); }

size_t scml_ffi_type_alignment(ScmlFFIType type) {
    switch (type) {
    case SCML_FFI_TYPE_BOOL:
    case SCML_FFI_TYPE_INT8:
    case SCML_FFI_TYPE_UINT8: return sizeof(uint8_t);
    case SCML_FFI_TYPE_INT16:
    case SCML_FFI_TYPE_UINT16: return sizeof(uint16_t);
    case SCML_FFI_TYPE_INT32:
    case SCML_FFI_TYPE_UINT32:
    case SCML_FFI_TYPE_FLOAT: return sizeof(uint32_t);
    case SCML_FFI_TYPE_INT64:
    case SCML_FFI_TYPE_UINT64:
    case SCML_FFI_TYPE_DOUBLE: return sizeof(uint64_t);
    case SCML_FFI_TYPE_POINTER:
    case SCML_FFI_TYPE_STRING: return sizeof(void *);
    case SCML_FFI_TYPE_SIZE: return sizeof(size_t);
    case SCML_FFI_TYPE_VOID:
    default: return 1;
    }
}

void *scml_ffi_alloc(size_t size) {
    if (size == 0) { ffi_set_error("alloc failed", "size must be greater than zero"); return NULL; }
    if (!ffi_ensure_memory_block_capacity(g_memory_block_count + 1)) return NULL;
    void *ptr = calloc(1, size);
    if (!ptr) { ffi_set_error("alloc failed", "out of memory"); return NULL; }
    g_memory_blocks[g_memory_block_count].ptr = ptr;
    g_memory_blocks[g_memory_block_count].size = size;
    g_memory_blocks[g_memory_block_count].owned = 1;
    g_memory_block_count++;
    return ptr;
}

void *scml_ffi_alloc_array(size_t count, size_t elem_size) {
    if (count == 0 || elem_size == 0) { ffi_set_error("alloc_array failed", "count and element size must be greater than zero"); return NULL; }
    if (count > ((size_t)-1) / elem_size) { ffi_set_error("alloc_array failed", "size overflow"); return NULL; }
    return scml_ffi_alloc(count * elem_size);
}

void *scml_ffi_realloc_memory(void *ptr, size_t new_size) {
    if (!ptr) return scml_ffi_alloc(new_size);
    if (new_size == 0) { ffi_set_error("realloc failed", "size must be greater than zero"); return NULL; }
    ScmlFFIMemoryBlock *block = ffi_memory_find(ptr);
    if (!block || !block->owned) { ffi_set_error("realloc failed", "pointer is not owned by FFI allocator"); return NULL; }
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr) { ffi_set_error("realloc failed", "out of memory"); return NULL; }
    if (new_size > block->size) memset((unsigned char *)new_ptr + block->size, 0, new_size - block->size);
    block->ptr = new_ptr;
    block->size = new_size;
    return new_ptr;
}

void *scml_ffi_alloc_cstring(const char *text) {
    size_t size = strlen(text ? text : "") + 1;
    char *ptr = (char *)scml_ffi_alloc(size);
    if (!ptr) return NULL;
    memcpy(ptr, text ? text : "", size);
    return ptr;
}

static size_t ffi_utf8_to_utf16_units(const char *text) {
    size_t units = 0;
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    while (*p) {
        uint32_t cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F); p += 4; }
        else { cp = 0xFFFD; p++; }
        units += cp > 0xFFFF ? 2 : 1;
    }
    return units;
}

void *scml_ffi_alloc_utf16(const char *text) {
    size_t units = ffi_utf8_to_utf16_units(text);
    if (units > (((size_t)-1) / sizeof(uint16_t)) - 1) { ffi_set_error("alloc_utf16 failed", "size overflow"); return NULL; }
    uint16_t *out = (uint16_t *)scml_ffi_alloc((units + 1) * sizeof(uint16_t));
    if (!out) return NULL;
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    size_t i = 0;
    while (*p) {
        uint32_t cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F); p += 4; }
        else { cp = 0xFFFD; p++; }
        if (cp > 0x10FFFF) cp = 0xFFFD;
        if (cp > 0xFFFF) {
            cp -= 0x10000;
            out[i++] = (uint16_t)(0xD800u + (cp >> 10));
            out[i++] = (uint16_t)(0xDC00u + (cp & 0x3FFu));
        } else {
            out[i++] = (uint16_t)cp;
        }
    }
    out[i] = 0;
    return out;
}

char *scml_ffi_read_utf16(const void *ptr, size_t max_code_units) {
    if (!ptr) { ffi_set_error("read_utf16 failed", "null pointer"); return NULL; }
    const uint16_t *in = (const uint16_t *)ptr;
    size_t limit = max_code_units ? max_code_units : 4096;
    if (!max_code_units) {
        ScmlFFIMemoryBlock *block = ffi_memory_find_containing(ptr, sizeof(uint16_t));
        if (block) {
            uintptr_t start = (uintptr_t)block->ptr;
            uintptr_t target = (uintptr_t)ptr;
            limit = (block->size - (size_t)(target - start)) / sizeof(uint16_t);
        }
    }
    size_t units = 0;
    while (units < limit && in[units] != 0) units++;
    if (units == limit) { ffi_set_error("read_utf16 failed", "unterminated string before safety limit"); return NULL; }
    if (units > (((size_t)-1) / 4) - 1) { ffi_set_error("read_utf16 failed", "size overflow"); return NULL; }
    char *out = (char *)malloc(units * 4 + 1);
    if (!out) { ffi_set_error("read_utf16 failed", "out of memory"); return NULL; }
    size_t j = 0;
    for (size_t i = 0; i < units; i++) {
        uint32_t cp = in[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units && in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
            cp = 0x10000 + (((cp - 0xD800) << 10) | (in[++i] - 0xDC00));
        }
        if (cp < 0x80) out[j++] = (char)cp;
        else if (cp < 0x800) { out[j++] = (char)(0xC0 | (cp >> 6)); out[j++] = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out[j++] = (char)(0xE0 | (cp >> 12)); out[j++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[j++] = (char)(0x80 | (cp & 0x3F)); }
        else { out[j++] = (char)(0xF0 | (cp >> 18)); out[j++] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[j++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[j++] = (char)(0x80 | (cp & 0x3F)); }
    }
    out[j] = '\0';
    return out;
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

int scml_ffi_write_cstring(void *base, size_t offset, const char *text, size_t max_bytes) {
    if (!base) { ffi_set_error("write_cstring failed", "null pointer"); return 0; }
    if (max_bytes == 0) { ffi_set_error("write_cstring failed", "max_bytes must be greater than zero"); return 0; }
    unsigned char *addr = (unsigned char *)base + offset;
    if (!ffi_memory_check_access(addr, max_bytes)) return 0;
    const char *src = text ? text : "";
    size_t n = strlen(src);
    if (n >= max_bytes) n = max_bytes - 1;
    memcpy(addr, src, n);
    addr[n] = '\0';
    return 1;
}

int scml_ffi_track_memory(void *ptr, size_t size) {
    if (!ptr) { ffi_set_error("track_memory failed", "null pointer"); return 0; }
    if (size == 0) { ffi_set_error("track_memory failed", "size must be greater than zero"); return 0; }
    ScmlFFIMemoryBlock *existing = ffi_memory_find(ptr);
    if (existing) {
        if (existing->owned) { ffi_set_error("track_memory failed", "pointer is already owned by FFI allocator"); return 0; }
        existing->size = size;
        return 1;
    }
    if (!ffi_ensure_memory_block_capacity(g_memory_block_count + 1)) return 0;
    g_memory_blocks[g_memory_block_count].ptr = ptr;
    g_memory_blocks[g_memory_block_count].size = size;
    g_memory_blocks[g_memory_block_count].owned = 0;
    g_memory_block_count++;
    return 1;
}

int scml_ffi_untrack_memory(void *ptr) {
    if (!ptr) { ffi_set_error("untrack_memory failed", "null pointer"); return 0; }
    for (size_t i = 0; i < g_memory_block_count; i++) {
        if (g_memory_blocks[i].ptr == ptr) {
            if (g_memory_blocks[i].owned) { ffi_set_error("untrack_memory failed", "pointer is owned by FFI allocator"); return 0; }
            if (i + 1 < g_memory_block_count) memmove(&g_memory_blocks[i], &g_memory_blocks[i + 1], (g_memory_block_count - i - 1) * sizeof(g_memory_blocks[0]));
            g_memory_block_count--;
            return 1;
        }
    }
    ffi_set_error("untrack_memory failed", "pointer is not tracked");
    return 0;
}

int scml_ffi_free_memory(void *ptr) {
    if (!ptr) { ffi_set_error("free failed", "null pointer"); return 0; }
    for (size_t i = 0; i < g_memory_block_count; i++) {
        if (g_memory_blocks[i].ptr == ptr) {
            if (!g_memory_blocks[i].owned) { ffi_set_error("free failed", "pointer is tracked but not owned by FFI allocator"); return 0; }
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

intptr_t scml_ffi_ptr_diff(const void *lhs, const void *rhs) {
    if (!lhs || !rhs) { ffi_set_error("ptr_diff failed", "null pointer"); return 0; }
    return (intptr_t)((const unsigned char *)lhs - (const unsigned char *)rhs);
}

int scml_ffi_memory_read(void *base, size_t offset, ScmlFFIType type, ScmlValue *ret) {
    if (!ret) { ffi_set_error("memory read failed", "missing output"); return 0; }
    unsigned char *addr = (unsigned char *)base + offset;
    size_t size = ffi_type_size(type);
    if (size == 0) { ffi_set_error("memory read failed", "void values are not addressable"); return 0; }
    if (!ffi_memory_check_access(addr, size)) return 0;
    switch (type) {
    case SCML_FFI_TYPE_BOOL:
    case SCML_FFI_TYPE_UINT8: { uint8_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_int((int32_t)v); return 1; }
    case SCML_FFI_TYPE_INT8: { int8_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_int((int32_t)v); return 1; }
    case SCML_FFI_TYPE_UINT16: { uint16_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_int((int32_t)v); return 1; }
    case SCML_FFI_TYPE_INT16: { int16_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_int((int32_t)v); return 1; }
    case SCML_FFI_TYPE_UINT32: { uint32_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_pointer((uintptr_t)v); return 1; }
    case SCML_FFI_TYPE_UINT64: { uint64_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_pointer((uintptr_t)v); return 1; }
    case SCML_FFI_TYPE_SIZE: { size_t v; memcpy(&v, addr, sizeof(v)); *ret = scml_value_pointer((uintptr_t)v); return 1; }
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
    if (size == 0) { ffi_set_error("memory write failed", "void values are not addressable"); return 0; }
    if (!ffi_memory_check_access(addr, size)) return 0;
    switch (type) {
    case SCML_FFI_TYPE_BOOL:
    case SCML_FFI_TYPE_UINT8: { uint8_t v = (uint8_t)ffi_uint64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT8: { int8_t v = (int8_t)ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_UINT16: { uint16_t v = (uint16_t)ffi_uint64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT16: { int16_t v = (int16_t)ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_UINT32: { uint32_t v = (uint32_t)ffi_uint64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_UINT64: { uint64_t v = (uint64_t)ffi_uint64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_SIZE: { size_t v = (size_t)ffi_uint64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_FLOAT: { float v = (float)ffi_double_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_DOUBLE: { double v = ffi_double_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT64: { int64_t v = ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_POINTER: { void *v = (void *)ffi_pointer_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_STRING: { const char *v = value->type == SCML_VAL_STRING ? (value->string ? value->string : "") : (const char *)ffi_pointer_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    case SCML_FFI_TYPE_INT32:
    default: { int32_t v = (int32_t)ffi_int64_value(value); memcpy(addr, &v, sizeof(v)); return 1; }
    }
}

int scml_ffi_array_read(void *base, size_t index, ScmlFFIType type, ScmlValue *ret) {
    size_t size = ffi_type_size(type);
    if (size == 0) { ffi_set_error("array read failed", "void elements are not addressable"); return 0; }
    if (index > ((size_t)-1) / size) { ffi_set_error("array read failed", "offset overflow"); return 0; }
    return scml_ffi_memory_read(base, index * size, type, ret);
}

int scml_ffi_array_write(void *base, size_t index, ScmlFFIType type, const ScmlValue *value) {
    size_t size = ffi_type_size(type);
    if (size == 0) { ffi_set_error("array write failed", "void elements are not addressable"); return 0; }
    if (index > ((size_t)-1) / size) { ffi_set_error("array write failed", "offset overflow"); return 0; }
    return scml_ffi_memory_write(base, index * size, type, value);
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

int scml_ffi_memory_compare(const void *lhs, const void *rhs, size_t size, int *out_cmp) {
    if (!out_cmp) { ffi_set_error("memcmp failed", "missing output"); return 0; }
    if (!ffi_memory_check_access(lhs, size) || !ffi_memory_check_access(rhs, size)) return 0;
    int cmp = memcmp(lhs, rhs, size);
    *out_cmp = (cmp > 0) - (cmp < 0);
    return 1;
}

static int ffi_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char *scml_ffi_read_bytes_hex(const void *base, size_t offset, size_t size) {
    if (!base) { ffi_set_error("read_bytes failed", "null pointer"); return NULL; }
    if (size > (((size_t)-1) - 1) / 2) { ffi_set_error("read_bytes failed", "size overflow"); return NULL; }
    const unsigned char *addr = (const unsigned char *)base + offset;
    if (!ffi_memory_check_access(addr, size)) return NULL;
    char *out = (char *)malloc(size * 2 + 1);
    if (!out) { ffi_set_error("read_bytes failed", "out of memory"); return NULL; }
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < size; i++) {
        out[i * 2] = hex[addr[i] >> 4];
        out[i * 2 + 1] = hex[addr[i] & 15];
    }
    out[size * 2] = '\0';
    return out;
}

int scml_ffi_write_bytes_hex(void *base, size_t offset, const char *hex_text) {
    if (!base) { ffi_set_error("write_bytes failed", "null pointer"); return 0; }
    if (!hex_text) { ffi_set_error("write_bytes failed", "missing hex text"); return 0; }
    size_t digits = 0;
    for (const char *p = hex_text; *p; p++) if (!isspace((unsigned char)*p)) digits++;
    if (digits % 2 != 0) { ffi_set_error("write_bytes failed", "hex text must contain an even number of digits"); return 0; }
    size_t size = digits / 2;
    unsigned char *addr = (unsigned char *)base + offset;
    if (!ffi_memory_check_access(addr, size)) return 0;
    int high = -1;
    size_t j = 0;
    for (const char *p = hex_text; *p; p++) {
        if (isspace((unsigned char)*p)) continue;
        int v = ffi_hex_nibble(*p);
        if (v < 0) { ffi_set_error("write_bytes failed", "invalid hex digit"); return 0; }
        if (high < 0) high = v;
        else { addr[j++] = (unsigned char)((high << 4) | v); high = -1; }
    }
    return 1;
}

size_t scml_ffi_memory_block_size(void *ptr) {
    ScmlFFIMemoryBlock *block = ffi_memory_find(ptr);
    return block ? block->size : 0;
}

static size_t ffi_align_up(size_t value, size_t alignment) {
    if (alignment <= 1) return value;
    size_t rem = value % alignment;
    return rem ? value + (alignment - rem) : value;
}

static ScmlFFIStructDef *ffi_struct_find(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g_struct_count; i++) if (g_structs[i].name && strcmp(g_structs[i].name, name) == 0) return &g_structs[i];
    return NULL;
}

static ScmlFFIStructField *ffi_struct_field_find(ScmlFFIStructDef *def, const char *field_name) {
    if (!def || !field_name) return NULL;
    for (size_t i = 0; i < def->field_count; i++) if (def->fields[i].name && strcmp(def->fields[i].name, field_name) == 0) return &def->fields[i];
    return NULL;
}

int scml_ffi_struct_begin(const char *struct_name) {
    if (!struct_name || !struct_name[0]) { ffi_set_error("struct_begin failed", "empty struct name"); return 0; }
    ScmlFFIStructDef *existing = ffi_struct_find(struct_name);
    if (existing) {
        for (size_t i = 0; i < existing->field_count; i++) free(existing->fields[i].name);
        g_struct_field_count -= existing->field_count;
        existing->field_count = 0;
        existing->size = 0;
        existing->alignment = 1;
        existing->finished = 0;
        existing->is_union = 0;
        return 1;
    }
    if (!ffi_ensure_struct_capacity(g_struct_count + 1)) return 0;
    char *owned_name = ffi_strdup(struct_name);
    if (!owned_name) { ffi_set_error("struct_begin failed", "out of memory"); return 0; }
    ScmlFFIStructDef *def = &g_structs[g_struct_count++];
    def->name = owned_name;
    def->fields = NULL;
    def->field_count = 0;
    def->field_capacity = 0;
    def->size = 0;
    def->alignment = 1;
    def->finished = 0;
    def->is_union = 0;
    return 1;
}

int scml_ffi_union_begin(const char *union_name) {
    if (!scml_ffi_struct_begin(union_name)) return 0;
    ScmlFFIStructDef *def = ffi_struct_find(union_name);
    if (!def) { ffi_set_error("union_begin failed", "unknown union"); return 0; }
    def->is_union = 1;
    return 1;
}

static int ffi_struct_add_field_count(const char *struct_name, const char *field_name, ScmlFFIType type, size_t element_count) {
    ScmlFFIStructDef *def = ffi_struct_find(struct_name);
    if (!def) { ffi_set_error("struct_add_field failed", "struct is not open"); return 0; }
    if (!field_name || !field_name[0]) { ffi_set_error("struct_add_field failed", "empty field name"); return 0; }
    if (def->finished) { ffi_set_error("struct_add_field failed", "struct is already finished"); return 0; }
    if (ffi_struct_field_find(def, field_name)) { ffi_set_error("struct_add_field failed", "duplicate field name"); return 0; }
    if (element_count == 0) { ffi_set_error("struct_add_field failed", "element count must be greater than zero"); return 0; }
    size_t element_size = ffi_type_size(type);
    if (element_size == 0) { ffi_set_error("struct_add_field failed", "void fields are not allowed"); return 0; }
    if (element_count > ((size_t)-1) / element_size) { ffi_set_error("struct_add_field failed", "field size overflow"); return 0; }
    size_t size = element_size * element_count;
    size_t alignment = scml_ffi_type_alignment(type);
    if (!ffi_ensure_struct_field_capacity(def, def->field_count + 1)) return 0;
    char *owned_name = ffi_strdup(field_name);
    if (!owned_name) { ffi_set_error("struct_add_field failed", "out of memory"); return 0; }
    size_t offset = def->is_union ? 0 : ffi_align_up(def->size, alignment);
    ScmlFFIStructField *field = &def->fields[def->field_count++];
    field->name = owned_name;
    field->type = type;
    field->offset = offset;
    field->size = size;
    field->alignment = alignment;
    field->element_size = element_size;
    field->element_count = element_count;
    if (def->is_union) {
        if (size > def->size) def->size = size;
    } else {
        def->size = offset + size;
    }
    if (alignment > def->alignment) def->alignment = alignment;
    g_struct_field_count++;
    return 1;
}

int scml_ffi_struct_add_field(const char *struct_name, const char *field_name, ScmlFFIType type) {
    return ffi_struct_add_field_count(struct_name, field_name, type, 1);
}

int scml_ffi_struct_add_array_field(const char *struct_name, const char *field_name, ScmlFFIType type, size_t element_count) {
    return ffi_struct_add_field_count(struct_name, field_name, type, element_count);
}

int scml_ffi_struct_finish(const char *struct_name) {
    ScmlFFIStructDef *def = ffi_struct_find(struct_name);
    if (!def) { ffi_set_error("struct_finish failed", "unknown struct"); return 0; }
    def->size = ffi_align_up(def->size, def->alignment);
    def->finished = 1;
    return 1;
}

int scml_ffi_struct_define_text(const char *struct_name, const char *field_spec) {
    if (!field_spec) { ffi_set_error("struct_define failed", "missing field spec"); return 0; }
    if (!scml_ffi_struct_begin(struct_name)) return 0;
    const char *p = field_spec;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *type_start = p;
        while (*p && *p != ':' && !isspace((unsigned char)*p)) p++;
        size_t type_len = (size_t)(p - type_start);
        while (isspace((unsigned char)*p)) p++;
        if (*p == ':') p++;
        while (isspace((unsigned char)*p)) p++;
        const char *name_start = p;
        while (*p && *p != ',') p++;
        size_t name_len = (size_t)(p - name_start);
        while (name_len > 0 && isspace((unsigned char)name_start[name_len - 1])) name_len--;
        char type_buf[32], name_buf[64];
        if (type_len == 0 || type_len >= sizeof(type_buf) || name_len == 0 || name_len >= sizeof(name_buf)) { ffi_set_error("struct_define failed", "bad field spec"); return 0; }
        memcpy(type_buf, type_start, type_len); type_buf[type_len] = '\0';
        memcpy(name_buf, name_start, name_len); name_buf[name_len] = '\0';
        size_t element_count = 1;
        char *type_array = strchr(type_buf, '[');
        char *name_array = strchr(name_buf, '[');
        char *array = type_array ? type_array : name_array;
        if (array) {
            char *end_array = strchr(array, ']');
            if (!end_array || end_array[1] != '\0') { ffi_set_error("struct_define failed", "bad array field spec"); return 0; }
            *array = '\0';
            *end_array = '\0';
            char *count_text = ffi_trim(array + 1);
            char *endptr = NULL;
            unsigned long parsed_count = strtoul(count_text, &endptr, 0);
            if (!count_text[0] || (endptr && *ffi_trim(endptr)) || parsed_count == 0) { ffi_set_error("struct_define failed", "bad array field count"); return 0; }
            element_count = (size_t)parsed_count;
        }
        if (!scml_ffi_struct_add_array_field(struct_name, ffi_trim(name_buf), scml_ffi_parse_type(ffi_trim(type_buf), SCML_FFI_TYPE_INT32), element_count)) return 0;
    }
    return scml_ffi_struct_finish(struct_name);
}

size_t scml_ffi_struct_size(const char *struct_name) {
    ScmlFFIStructDef *def = ffi_struct_find(struct_name);
    return def ? ffi_align_up(def->size, def->alignment) : 0;
}

size_t scml_ffi_struct_alignment(const char *struct_name) {
    ScmlFFIStructDef *def = ffi_struct_find(struct_name);
    return def ? def->alignment : 0;
}

int scml_ffi_struct_field_info(const char *struct_name, const char *field_name, ScmlFFIFieldInfo *out_info) {
    ScmlFFIStructField *field = ffi_struct_field_find(ffi_struct_find(struct_name), field_name);
    if (!field) { ffi_set_error("struct_field_info failed", "unknown field"); return 0; }
    if (out_info) {
        out_info->name = field->name;
        out_info->type = field->type;
        out_info->offset = field->offset;
        out_info->size = field->size;
        out_info->alignment = field->alignment;
        out_info->element_size = field->element_size;
        out_info->element_count = field->element_count;
    }
    return 1;
}

static int ffi_struct_field_index_address(void *base, const char *struct_name, const char *field_name, size_t element_index, ScmlFFIFieldInfo *out_info, void **out_ptr) {
    if (!base) { ffi_set_error("struct field index failed", "null base pointer"); return 0; }
    ScmlFFIFieldInfo info;
    if (!scml_ffi_struct_field_info(struct_name, field_name, &info)) return 0;
    if (element_index >= info.element_count) { ffi_set_error("struct field index failed", "array field index out of range"); return 0; }
    if (out_info) *out_info = info;
    if (out_ptr) *out_ptr = (unsigned char *)base + info.offset + element_index * info.element_size;
    return 1;
}

void *scml_ffi_struct_field_element_ptr(void *base, const char *struct_name, const char *field_name, size_t element_index) {
    ScmlFFIFieldInfo info;
    void *ptr = NULL;
    if (!ffi_struct_field_index_address(base, struct_name, field_name, element_index, &info, &ptr)) return NULL;
    if (!ffi_memory_check_access(ptr, info.element_size)) return NULL;
    return ptr;
}

int scml_ffi_struct_field_read_index(void *base, const char *struct_name, const char *field_name, size_t element_index, ScmlValue *ret) {
    ScmlFFIFieldInfo info;
    void *ptr = NULL;
    if (!ffi_struct_field_index_address(base, struct_name, field_name, element_index, &info, &ptr)) return 0;
    if (!ffi_memory_check_access(ptr, info.element_size)) return 0;
    return scml_ffi_memory_read(ptr, 0, info.type, ret);
}

int scml_ffi_struct_field_write_index(void *base, const char *struct_name, const char *field_name, size_t element_index, const ScmlValue *value) {
    ScmlFFIFieldInfo info;
    void *ptr = NULL;
    if (!ffi_struct_field_index_address(base, struct_name, field_name, element_index, &info, &ptr)) return 0;
    if (!ffi_memory_check_access(ptr, info.element_size)) return 0;
    return scml_ffi_memory_write(ptr, 0, info.type, value);
}

int scml_ffi_struct_read(void *base, const char *struct_name, const char *field_name, ScmlValue *ret) {
    ScmlFFIFieldInfo info;
    if (!scml_ffi_struct_field_info(struct_name, field_name, &info)) return 0;
    return scml_ffi_memory_read(base, info.offset, info.type, ret);
}

int scml_ffi_struct_write(void *base, const char *struct_name, const char *field_name, const ScmlValue *value) {
    ScmlFFIFieldInfo info;
    if (!scml_ffi_struct_field_info(struct_name, field_name, &info)) return 0;
    return scml_ffi_memory_write(base, info.offset, info.type, value);
}

static int ffi_struct_array_base(void *base, size_t index, const char *struct_name, void **out_ptr) {
    if (!out_ptr) return 0;
    if (!base) { ffi_set_error("struct array access failed", "null base pointer"); return 0; }
    size_t struct_size = scml_ffi_struct_size(struct_name);
    if (struct_size == 0) { ffi_set_error("struct array access failed", "unknown or empty struct"); return 0; }
    if (index > ((size_t)-1) / struct_size) { ffi_set_error("struct array access failed", "offset overflow"); return 0; }
    *out_ptr = (unsigned char *)base + index * struct_size;
    return 1;
}

void *scml_ffi_struct_ptr(void *base, const char *struct_name, size_t index) {
    void *ptr = NULL;
    if (!ffi_struct_array_base(base, index, struct_name, &ptr)) return NULL;
    if (!ffi_memory_check_access(ptr, scml_ffi_struct_size(struct_name))) return NULL;
    return ptr;
}

void *scml_ffi_struct_field_ptr(void *base, const char *struct_name, const char *field_name) {
    if (!base) { ffi_set_error("struct_field_ptr failed", "null base pointer"); return NULL; }
    ScmlFFIFieldInfo info;
    if (!scml_ffi_struct_field_info(struct_name, field_name, &info)) return NULL;
    void *ptr = (unsigned char *)base + info.offset;
    if (!ffi_memory_check_access(ptr, info.size)) return NULL;
    return ptr;
}

void *scml_ffi_alloc_struct(const char *struct_name) {
    size_t size = scml_ffi_struct_size(struct_name);
    if (size == 0) { ffi_set_error("alloc_struct failed", "unknown or empty struct"); return NULL; }
    return scml_ffi_alloc(size);
}

void *scml_ffi_alloc_struct_array(const char *struct_name, size_t count) {
    size_t size = scml_ffi_struct_size(struct_name);
    if (size == 0) { ffi_set_error("alloc_struct_array failed", "unknown or empty struct"); return NULL; }
    return scml_ffi_alloc_array(count, size);
}

int scml_ffi_struct_array_read(void *base, size_t index, const char *struct_name, const char *field_name, ScmlValue *ret) {
    void *ptr = NULL;
    if (!ffi_struct_array_base(base, index, struct_name, &ptr)) return 0;
    return scml_ffi_struct_read(ptr, struct_name, field_name, ret);
}

int scml_ffi_struct_array_write(void *base, size_t index, const char *struct_name, const char *field_name, const ScmlValue *value) {
    void *ptr = NULL;
    if (!ffi_struct_array_base(base, index, struct_name, &ptr)) return 0;
    return scml_ffi_struct_write(ptr, struct_name, field_name, value);
}

int scml_ffi_struct_undefine(const char *struct_name) {
    for (size_t i = 0; i < g_struct_count; i++) {
        if (g_structs[i].name && strcmp(g_structs[i].name, struct_name) == 0) {
            free(g_structs[i].name);
            for (size_t f = 0; f < g_structs[i].field_count; f++) free(g_structs[i].fields[f].name);
            g_struct_field_count -= g_structs[i].field_count;
            free(g_structs[i].fields);
            if (i + 1 < g_struct_count) memmove(&g_structs[i], &g_structs[i + 1], (g_struct_count - i - 1) * sizeof(g_structs[0]));
            g_struct_count--;
            return 1;
        }
    }
    ffi_set_error("struct_undefine failed", "unknown struct");
    return 0;
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
    out_stats->struct_count = g_struct_count;
    out_stats->struct_capacity = g_struct_capacity;
    out_stats->struct_field_count = g_struct_field_count;
    out_stats->struct_field_capacity = g_struct_field_capacity;
}

void scml_ffi_shutdown(void) {
    for (size_t i = 0; i < g_symbol_count; i++) free(g_symbols[i].name);
    free(g_symbols);
    g_symbols = NULL;
    g_symbol_count = 0;
    g_symbol_capacity = 0;
    for (size_t i = 0; i < g_signature_count; i++) { free(g_signatures[i].name); free(g_signatures[i].symbol_name); free(g_signatures[i].arg_types); }
    free(g_signatures);
    g_signatures = NULL;
    g_signature_count = 0;
    g_signature_capacity = 0;
    for (size_t i = 0; i < g_search_path_count; i++) free(g_search_paths[i].path);
    free(g_search_paths);
    g_search_paths = NULL;
    g_search_path_count = 0;
    g_search_path_capacity = 0;
    for (size_t i = 0; i < g_memory_block_count; i++) if (g_memory_blocks[i].owned) free(g_memory_blocks[i].ptr);
    free(g_memory_blocks);
    g_memory_blocks = NULL;
    g_memory_block_count = 0;
    g_memory_block_capacity = 0;
    for (size_t i = 0; i < g_struct_count; i++) {
        free(g_structs[i].name);
        for (size_t f = 0; f < g_structs[i].field_count; f++) free(g_structs[i].fields[f].name);
        free(g_structs[i].fields);
    }
    free(g_structs);
    g_structs = NULL;
    g_struct_count = 0;
    g_struct_capacity = 0;
    g_struct_field_count = 0;
    g_struct_field_capacity = 0;
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
