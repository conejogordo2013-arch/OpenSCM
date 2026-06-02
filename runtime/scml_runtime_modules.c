#define _POSIX_C_SOURCE 200809L
#include "scml_runtime_modules.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#endif

#if defined(SCML_USE_SDL2)
#include <SDL2/SDL.h>
#endif
#if defined(SCML_USE_OPENGL)
#include <GL/gl.h>
#endif
#if defined(SCML_USE_OPENGLES)
#include <GLES2/gl2.h>
#endif
#if defined(SCML_USE_VULKAN)
#include <vulkan/vulkan.h>
#endif
#if defined(SCML_USE_D3D11) || defined(SCML_USE_D3D12)
#include <d3d11.h>
#include <d3d12.h>
#endif
#if defined(SCML_USE_METAL)
#include <Metal/Metal.h>
#endif

#define SCML_RUNTIME_MODULES_MAX 32
#define SCML_RUNTIME_BACKENDS_MAX 16

typedef struct RuntimeBackend {
    char *name;
    const ScmlRuntimeFunctionEntry *functions;
    size_t function_count;
    void *user_data;
} RuntimeBackend;

typedef struct RuntimeModule {
    char *name;
    RuntimeBackend backends[SCML_RUNTIME_BACKENDS_MAX];
    size_t backend_count;
    size_t active_backend;
} RuntimeModule;

typedef struct RuntimeRegistry {
    RuntimeModule modules[SCML_RUNTIME_MODULES_MAX];
    size_t count;
} RuntimeRegistry;

static RuntimeRegistry g_registry = {0};
typedef struct RuntimeBuiltinState {
    int window_alive, window_visible, window_w, window_h, close_requested;
    int gpu_context_alive, frame_active, next_handle;
    int key_down[256], mouse_down[8], mouse_x, mouse_y, mouse_wheel;
    int audio_volume, socket_open;
    int socket_fd;
    char last_net_payload[1024];
    int thread_active[64];
    int thread_done[64];
#if !defined(_WIN32)
    pthread_t thread_handles[64];
#endif
#if defined(SCML_USE_OPENGL)
    GLuint active_vbo;
#endif
#if defined(SCML_USE_SDL2)
    SDL_Window *window;
    SDL_GLContext glctx;
#endif
} RuntimeBuiltinState;
static RuntimeBuiltinState g_builtin = {0};

static char *scml_runtime_strdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s ? s : "", n + 1);
    return r;
}

static RuntimeModule *runtime_find(const char *name) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.modules[i].name, name) == 0) return &g_registry.modules[i];
    }
    return NULL;
}

static int runtime_dispatch(ScmlVM *vm, const char *function, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    RuntimeModule *module = (RuntimeModule *)user_data;
    if (!module || !function) return 0;
    if (module->active_backend >= module->backend_count) return 0;
    RuntimeBackend *backend = &module->backends[module->active_backend];
    for (size_t i = 0; i < backend->function_count; i++) {
        if (strcmp(backend->functions[i].name, function) == 0) {
            return backend->functions[i].fn(vm, args, arg_count, ret, backend->user_data);
        }
    }
    return 0;
}

int scml_runtime_register_module(ScmlVM *vm,
                                 const char *module_name,
                                 const ScmlRuntimeFunctionEntry *functions,
                                 size_t function_count,
                                 void *user_data) {
    if (!vm || !module_name || !functions || function_count == 0) return 0;
    RuntimeModule *module = runtime_find(module_name);
    if (!module) {
        if (g_registry.count >= SCML_RUNTIME_MODULES_MAX) return 0;
        module = &g_registry.modules[g_registry.count++];
        module->name = scml_runtime_strdup(module_name);
        if (!module->name) return 0;
    }
    module->backend_count = 0;
    module->active_backend = 0;
    ScmlRuntimeBackendVTable default_backend = {"default", functions, function_count, user_data};
    return scml_runtime_register_backend(vm, module_name, &default_backend);
}

int scml_runtime_register_backend(ScmlVM *vm, const char *module_name, const ScmlRuntimeBackendVTable *backend) {
    if (!vm || !module_name || !backend || !backend->backend_name || !backend->functions || backend->function_count == 0) return 0;
    RuntimeModule *module = runtime_find(module_name);
    if (!module) {
        if (g_registry.count >= SCML_RUNTIME_MODULES_MAX) return 0;
        module = &g_registry.modules[g_registry.count++];
        module->name = scml_runtime_strdup(module_name);
        if (!module->name) return 0;
        module->backend_count = 0;
        module->active_backend = 0;
        if (!scml_vm_register_module(vm, module_name, runtime_dispatch, module)) return 0;
    }
    for (size_t i = 0; i < module->backend_count; i++) {
        if (strcmp(module->backends[i].name, backend->backend_name) == 0) {
            module->backends[i].functions = backend->functions;
            module->backends[i].function_count = backend->function_count;
            module->backends[i].user_data = backend->user_data;
            return 1;
        }
    }
    if (module->backend_count >= SCML_RUNTIME_BACKENDS_MAX) return 0;
    RuntimeBackend *slot = &module->backends[module->backend_count++];
    slot->name = scml_runtime_strdup(backend->backend_name);
    if (!slot->name) return 0;
    slot->functions = backend->functions;
    slot->function_count = backend->function_count;
    slot->user_data = backend->user_data;
    return 1;
}

int scml_runtime_select_backend(ScmlVM *vm, const char *module_name, const char *backend_name) {
    (void)vm;
    RuntimeModule *module = runtime_find(module_name);
    if (!module || !backend_name) return 0;
    for (size_t i = 0; i < module->backend_count; i++) {
        if (strcmp(module->backends[i].name, backend_name) == 0) {
            module->active_backend = i;
            return 1;
        }
    }
    return 0;
}

static int scml_runtime_register_builtin_backend(ScmlVM *vm,
                                                const char *module_name,
                                                const char *backend_name,
                                                const ScmlRuntimeFunctionEntry *functions,
                                                size_t function_count) {
    ScmlRuntimeBackendVTable backend = {backend_name, functions, function_count, (void *)module_name};
    return scml_runtime_register_backend(vm, module_name, &backend);
}

static const char *scml_runtime_active_backend_name(const char *module_name) {
    RuntimeModule *module = runtime_find(module_name);
    if (!module || module->active_backend >= module->backend_count) return "";
    return module->backends[module->active_backend].name ? module->backends[module->active_backend].name : "";
}

static void scml_runtime_append(char *buf, size_t buf_size, const char *text) {
    size_t used = strlen(buf);
    if (used < buf_size) snprintf(buf + used, buf_size - used, "%s", text ? text : "");
}

static void scml_runtime_append_compile_flags(char *buf, size_t buf_size) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "compiled={SDL2:%d,OpenGL:%d,OpenGLES:%d,Vulkan:%d,D3D11:%d,D3D12:%d,Metal:%d}",
#if defined(SCML_USE_SDL2)
             1,
#else
             0,
#endif
#if defined(SCML_USE_OPENGL)
             1,
#else
             0,
#endif
#if defined(SCML_USE_OPENGLES)
             1,
#else
             0,
#endif
#if defined(SCML_USE_VULKAN)
             1,
#else
             0,
#endif
#if defined(SCML_USE_D3D11)
             1,
#else
             0,
#endif
#if defined(SCML_USE_D3D12)
             1,
#else
             0,
#endif
#if defined(SCML_USE_METAL)
             1
#else
             0
#endif
    );
    scml_runtime_append(buf, buf_size, tmp);
}

static int scml_runtime_backend_info(char *buf, size_t buf_size, const char *module_name) {
    RuntimeModule *module = runtime_find(module_name);
    if (!module || !buf || buf_size == 0) return 0;
    snprintf(buf, buf_size, "module=%s active=%s backends=", module_name, scml_runtime_active_backend_name(module_name));
    for (size_t i = 0; i < module->backend_count; i++) {
        if (i) scml_runtime_append(buf, buf_size, ",");
        scml_runtime_append(buf, buf_size, module->backends[i].name);
    }
    scml_runtime_append(buf, buf_size, " ");
    scml_runtime_append_compile_flags(buf, buf_size);
    return 1;
}

int scml_runtime_unregister_module(ScmlVM *vm, const char *module_name) {
    if (!vm || !module_name) return 0;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.modules[i].name, module_name) == 0) {
            RuntimeModule *module = &g_registry.modules[i];
            for (size_t bi = 0; bi < module->backend_count; bi++) free(module->backends[bi].name);
            free(module->name);
            if (i + 1 < g_registry.count) memmove(&g_registry.modules[i], &g_registry.modules[i + 1], (g_registry.count - i - 1) * sizeof(g_registry.modules[0]));
            g_registry.count--;
            return scml_vm_unregister_module(vm, module_name);
        }
    }
    return 0;
}

int scml_runtime_resolve_module(ScmlVM *vm, const char *module_name) {
    (void)vm;
    return runtime_find(module_name) != NULL;
}

static const char *arg_to_cstr(const ScmlValue *v, char *buf, size_t n) {
    if (!v) return "";
    if (v->type == SCML_VAL_STRING) return v->string ? v->string : "";
    if (v->type == SCML_VAL_FLOAT) { snprintf(buf, n, "%g", v->real); return buf; }
    snprintf(buf, n, "%d", v->integer);
    return buf;
}

static void scml_runtime_sleep_us(unsigned int usec) {
#if defined(_WIN32)
    DWORD ms = (DWORD)((usec + 999u) / 1000u);
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(usec / 1000000u);
    ts.tv_nsec = (long)(usec % 1000000u) * 1000L;
    nanosleep(&ts, NULL);
#endif
}

static int is_unreserved_url_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int rt_data_hash32(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char buf[256];
    const unsigned char *p = (const unsigned char *)arg_to_cstr(&args[0], buf, sizeof(buf));
    uint32_t hash = 2166136261u;
    while (*p) { hash ^= (uint32_t)*p++; hash *= 16777619u; }
    *ret = scml_value_int((int32_t)(hash & 0x7fffffffu));
    return 1;
}

static int rt_data_url_encode(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char buf[512];
    const unsigned char *src = (const unsigned char *)arg_to_cstr(&args[0], buf, sizeof(buf));
    size_t len = strlen((const char *)src);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return 0;
    size_t j = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; src[i]; i++) {
        if (is_unreserved_url_char(src[i])) out[j++] = (char)src[i];
        else { out[j++] = '%'; out[j++] = hex[src[i] >> 4]; out[j++] = hex[src[i] & 15]; }
    }
    out[j] = '\0';
    *ret = scml_value_string(out);
    free(out);
    return 1;
}

static int rt_data_url_decode(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char buf[512];
    const char *src = arg_to_cstr(&args[0], buf, sizeof(buf));
    char *out = (char *)malloc(strlen(src) + 1);
    if (!out) return 0;
    size_t j = 0;
    for (size_t i = 0; src[i]; i++) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            out[j++] = (char)((hex_value(src[i + 1]) << 4) | hex_value(src[i + 2]));
            i += 2;
        } else if (src[i] == '+') out[j++] = ' ';
        else out[j++] = src[i];
    }
    out[j] = '\0';
    *ret = scml_value_string(out);
    free(out);
    return 1;
}

static int rt_data_split(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 3) return 0;
    char sbuf[1024], dbuf[64];
    const char *src = arg_to_cstr(&args[0], sbuf, sizeof(sbuf));
    const char *delim = arg_to_cstr(&args[1], dbuf, sizeof(dbuf));
    int wanted = args[2].type == SCML_VAL_FLOAT ? (int)args[2].real : args[2].integer;
    if (!*delim || wanted < 0) return 0;
    size_t dlen = strlen(delim);
    const char *start = src;
    int index = 0;
    while (1) {
        const char *next = strstr(start, delim);
        if (index == wanted) {
            size_t n = next ? (size_t)(next - start) : strlen(start);
            char *piece = (char *)malloc(n + 1);
            if (!piece) return 0;
            memcpy(piece, start, n); piece[n] = '\0';
            *ret = scml_value_string(piece);
            free(piece);
            return 1;
        }
        if (!next) break;
        start = next + dlen;
        index++;
    }
    *ret = scml_value_string("");
    return 1;
}

static const char *json_find_key_value(const char *json, const char *key) {
    size_t klen = strlen(key);
    for (const char *p = json; (p = strchr(p, '"')) != NULL; p++) {
        p++;
        if (strncmp(p, key, klen) != 0 || p[klen] != '"') continue;
        p += klen + 1;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != ':') continue;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        return p;
    }
    return NULL;
}

static int rt_data_json_get(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 2) return 0;
    char jbuf[2048], kbuf[256];
    const char *json = arg_to_cstr(&args[0], jbuf, sizeof(jbuf));
    const char *key = arg_to_cstr(&args[1], kbuf, sizeof(kbuf));
    const char *v = json_find_key_value(json, key);
    if (!v) { *ret = scml_value_string(""); return 1; }
    if (*v == '"') {
        v++;
        char out[1024]; size_t j = 0;
        while (*v && *v != '"' && j + 1 < sizeof(out)) {
            if (*v == '\\' && v[1]) v++;
            out[j++] = *v++;
        }
        out[j] = '\0';
        *ret = scml_value_string(out);
        return 1;
    }
    if (strncmp(v, "true", 4) == 0) { *ret = scml_value_int(1); return 1; }
    if (strncmp(v, "false", 5) == 0 || strncmp(v, "null", 4) == 0) { *ret = scml_value_int(0); return 1; }
    char *end = NULL;
    double number = strtod(v, &end);
    if (end != v) {
        if (strchr(v, '.') || strchr(v, 'e') || strchr(v, 'E')) *ret = scml_value_float((float)number);
        else *ret = scml_value_int((int32_t)number);
        return 1;
    }
    *ret = scml_value_string("");
    return 1;
}

static int rt_env_get(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char nbuf[256];
    const char *name = arg_to_cstr(&args[0], nbuf, sizeof(nbuf));
    const char *value = getenv(name);
    *ret = scml_value_string(value ? value : "");
    return 1;
}

static int rt_env_set(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 2) return 0;
    char nbuf[256], vbuf[512];
    const char *name = arg_to_cstr(&args[0], nbuf, sizeof(nbuf));
    const char *value = arg_to_cstr(&args[1], vbuf, sizeof(vbuf));
#if defined(_WIN32)
    int ok = _putenv_s(name, value) == 0;
#else
    int ok = setenv(name, value, 1) == 0;
#endif
    *ret = scml_value_int(ok ? 1 : 0);
    return 1;
}


static int rt_file_read_txt(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char pbuf[512];
    const char *path = arg_to_cstr(&args[0], pbuf, sizeof(pbuf));
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n < 0) { fclose(f); return 0; }
    char *data = (char *)malloc((size_t)n + 1);
    if (!data) { fclose(f); return 0; }
    size_t got = fread(data, 1, (size_t)n, f);
    fclose(f);
    data[got] = '\0';
    *ret = scml_value_string(data);
    free(data);
    return 1;
}

static int rt_file_write_txt(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 2) return 0;
    char pbuf[512], dbuf[128];
    const char *path = arg_to_cstr(&args[0], pbuf, sizeof(pbuf));
    const char *text = arg_to_cstr(&args[1], dbuf, sizeof(dbuf));
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fputs(text, f);
    fclose(f);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_audio_play_sound(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;
    *ret = scml_value_int(arg_count > 0 ? args[0].integer : 0);
    return 1;
#else
    *ret = scml_value_int(arg_count > 0 ? args[0].integer : 0);
    return 1;
#endif
}

static int rt_net_open_socket(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    g_builtin.socket_open = 1;
    *ret = scml_value_int(++g_builtin.next_handle);
#else
    g_builtin.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_builtin.socket_fd < 0) return 0;
    g_builtin.socket_open = 1;
    *ret = scml_value_int(g_builtin.socket_fd);
#endif
    return 1;
}
static int rt_net_close_socket(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if !defined(_WIN32)
    if (g_builtin.socket_open && g_builtin.socket_fd >= 0) close(g_builtin.socket_fd);
    g_builtin.socket_fd = -1;
#endif
    g_builtin.socket_open = 0;
    *ret = scml_value_int(0);
    return 1;
}
static int rt_net_connect(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.socket_open || arg_count < 2) return 0;
#if !defined(_WIN32)
    char hbuf[256], pbuf[64];
    const char *host = arg_to_cstr(&args[0], hbuf, sizeof(hbuf));
    const char *port = arg_to_cstr(&args[1], pbuf, sizeof(pbuf));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(port));
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) return 0;
    if (connect(g_builtin.socket_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) return 0;
#endif
    *ret = scml_value_int(0);
    return 1;
}

static int rt_net_send_data(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.socket_open || arg_count < 2) return 0;
    char b[1024];
    const char *p = arg_to_cstr(&args[1], b, sizeof(b));
    snprintf(g_builtin.last_net_payload, sizeof(g_builtin.last_net_payload), "%s", p);
#if !defined(_WIN32)
    ssize_t sent = send(g_builtin.socket_fd, g_builtin.last_net_payload, strlen(g_builtin.last_net_payload), 0);
    if (sent < 0) return 0;
    *ret = scml_value_int((int32_t)sent);
    return 1;
#else
    *ret = scml_value_int((int32_t)strlen(g_builtin.last_net_payload));
    return 1;
#endif
}
static int rt_net_receive_data(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    if (!g_builtin.socket_open) return 0;
#if !defined(_WIN32)
    char buf[1024];
    ssize_t n = recv(g_builtin.socket_fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
    if (n > 0) {
        buf[n] = '\0';
        snprintf(g_builtin.last_net_payload, sizeof(g_builtin.last_net_payload), "%s", buf);
    }
#endif
    *ret = scml_value_string(g_builtin.last_net_payload);
    return 1;
}
static int rt_net_request(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char u[256], out[1200];
    const char *url = arg_to_cstr(&args[0], u, sizeof(u));
    snprintf(out, sizeof(out), "{\"ok\":true,\"url\":\"%s\"}", url);
    *ret = scml_value_string(out);
    return 1;
}

static int rt_window_create(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    g_builtin.window_alive = 1; g_builtin.window_visible = 0; g_builtin.close_requested = 0;
    g_builtin.window_w = arg_count > 0 ? args[0].integer : 800;
    g_builtin.window_h = arg_count > 1 ? args[1].integer : 600;
#if defined(SCML_USE_SDL2)
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return 0;
    if (!g_builtin.window) {
        g_builtin.window = SDL_CreateWindow("SCML", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_builtin.window_w, g_builtin.window_h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!g_builtin.window) return 0;
    }
#endif
    *ret = scml_value_int(0); return 1;
}
static int rt_window_destroy(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    #if defined(SCML_USE_SDL2)
    if (g_builtin.glctx) { SDL_GL_DeleteContext(g_builtin.glctx); g_builtin.glctx = NULL; }
    if (g_builtin.window) { SDL_DestroyWindow(g_builtin.window); g_builtin.window = NULL; }
    #endif
    g_builtin.window_alive = 0; g_builtin.window_visible = 0; *ret = scml_value_int(0); return 1;
}
static int rt_window_show(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (g_builtin.window) SDL_ShowWindow(g_builtin.window);
#endif
    g_builtin.window_visible = 1; *ret = scml_value_int(0); return 1;
}
static int rt_window_hide(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (g_builtin.window) SDL_HideWindow(g_builtin.window);
#endif
    g_builtin.window_visible = 0; *ret = scml_value_int(0); return 1;
}
static int rt_window_resize(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data; if (arg_count < 2) return 0; g_builtin.window_w = args[0].integer; g_builtin.window_h = args[1].integer;
#if defined(SCML_USE_SDL2)
    if (g_builtin.window) SDL_SetWindowSize(g_builtin.window, g_builtin.window_w, g_builtin.window_h);
#endif
    *ret = scml_value_int(0); return 1;
}
static int rt_window_close_requested(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(g_builtin.close_requested); return 1;
}
static int rt_gpu_create_context(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (g_builtin.window && !g_builtin.glctx) {
        g_builtin.glctx = SDL_GL_CreateContext(g_builtin.window);
        if (!g_builtin.glctx) return 0;
        SDL_GL_MakeCurrent(g_builtin.window, g_builtin.glctx);
    }
#endif
    g_builtin.gpu_context_alive = 1; *ret = scml_value_int(0); return 1;
}
static int rt_gpu_destroy_context(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (g_builtin.glctx) { SDL_GL_DeleteContext(g_builtin.glctx); g_builtin.glctx = NULL; }
#endif
    g_builtin.gpu_context_alive = 0; *ret = scml_value_int(0); return 1;
}
static int rt_gpu_begin_frame(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data; if (!g_builtin.gpu_context_alive) return 0; g_builtin.frame_active = 1; *ret = scml_value_int(0); return 1;
}
static int rt_gpu_present(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    if (g_builtin.window && g_builtin.glctx) SDL_GL_SwapWindow(g_builtin.window);
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_gpu_end_frame(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    g_builtin.frame_active = 0; *ret = scml_value_int(0); return 1;
}
static int rt_create_handle(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(++g_builtin.next_handle); return 1;
}
static int rt_gpu_create_texture2d(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    if (!g_builtin.gpu_context_alive) return 0;
#if defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES)
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (tex == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    *ret = scml_value_int((int32_t)tex);
    return 1;
#else
    return rt_create_handle(vm, args, arg_count, ret, user_data);
#endif
}
static int rt_gpu_update_texture2d(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.gpu_context_alive || arg_count < 4) return 0;
    int tex = args[0].integer;
    int w = args[1].integer;
    int h = args[2].integer;
    if (w <= 0 || h <= 0) return 0;
#if !(defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES))
    (void)tex;
#endif
#if defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES)
    if (tex <= 0) return 0;
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    if (args[3].type == SCML_VAL_STRING && args[3].string) {
        const unsigned char *px = (const unsigned char *)args[3].string;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_gpu_destroy_texture(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
#if !(defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES))
    (void)args;
#endif
#if defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES)
    if (args[0].integer > 0) {
        GLuint tex = (GLuint)args[0].integer;
        glDeleteTextures(1, &tex);
    }
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_file_exists(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data; if (arg_count < 1) return 0; char p[512]; const char *path = arg_to_cstr(&args[0], p, sizeof(p)); FILE *f = fopen(path, "rb"); *ret = scml_value_int(f ? 1 : 0); if (f) fclose(f); return 1;
}
static int rt_file_delete_file(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data; if (arg_count < 1) return 0; char p[512]; const char *path = arg_to_cstr(&args[0], p, sizeof(p)); *ret = scml_value_int(remove(path) == 0 ? 0 : -1); return 1;
}
static int rt_file_create_directory(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data; if (arg_count < 1) return 0; char p[512]; const char *path = arg_to_cstr(&args[0], p, sizeof(p));
#if defined(_WIN32)
    *ret = scml_value_int(CreateDirectoryA(path, NULL) ? 0 : -1);
#else
    *ret = scml_value_int(mkdir(path, 0755) == 0 ? 0 : -1);
#endif
    return 1;
}
static int rt_runtime_get_time_ms(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    *ret = scml_value_int((int32_t)GetTickCount());
#else
    struct timeval tv; gettimeofday(&tv, NULL); long long ms = (long long)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
    *ret = scml_value_int((int32_t)(ms & 0x7fffffff));
#endif
    return 1;
}
static int rt_runtime_get_time_us(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    *ret = scml_value_int((int32_t)GetTickCount() * 1000);
#else
    struct timeval tv; gettimeofday(&tv, NULL); long long us = (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
    *ret = scml_value_int((int32_t)(us & 0x7fffffff));
#endif
    return 1;
}
static int rt_runtime_get_ticks(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { return rt_runtime_get_time_ms(vm, args, arg_count, ret, user_data); }
static int rt_input_poll_events(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(SCML_USE_SDL2)
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) g_builtin.close_requested = 1;
        else if (ev.type == SDL_MOUSEMOTION) { g_builtin.mouse_x = ev.motion.x; g_builtin.mouse_y = ev.motion.y; }
        else if (ev.type == SDL_MOUSEWHEEL) g_builtin.mouse_wheel += ev.wheel.y;
        else if (ev.type == SDL_KEYDOWN) g_builtin.key_down[ev.key.keysym.scancode & 255] = 1;
        else if (ev.type == SDL_KEYUP) g_builtin.key_down[ev.key.keysym.scancode & 255] = 0;
        else if (ev.type == SDL_MOUSEBUTTONDOWN) g_builtin.mouse_down[ev.button.button & 7] = 1;
        else if (ev.type == SDL_MOUSEBUTTONUP) g_builtin.mouse_down[ev.button.button & 7] = 0;
    }
#endif
    *ret = scml_value_int(0); return 1;
}
static int rt_input_is_key_down(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 1) return 0; int k = args[0].integer & 255; *ret = scml_value_int(g_builtin.key_down[k]); return 1; }
static int rt_input_is_key_up(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 1) return 0; int k = args[0].integer & 255; *ret = scml_value_int(!g_builtin.key_down[k]); return 1; }
static int rt_input_get_mouse_position(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; char b[64]; snprintf(b, sizeof(b), "%d,%d", g_builtin.mouse_x, g_builtin.mouse_y); *ret = scml_value_string(b); return 1; }
static int rt_input_is_mouse_button_down(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 1) return 0; int b = args[0].integer & 7; *ret = scml_value_int(g_builtin.mouse_down[b]); return 1; }
static int rt_input_is_mouse_button_up(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 1) return 0; int b = args[0].integer & 7; *ret = scml_value_int(!g_builtin.mouse_down[b]); return 1; }
static int rt_input_get_mouse_wheel(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(g_builtin.mouse_wheel); g_builtin.mouse_wheel = 0; return 1; }
static int rt_window_set_title(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char t[256];
    const char *title = arg_to_cstr(&args[0], t, sizeof(t));
#if !defined(SCML_USE_SDL2)
    (void)title;
#endif
#if defined(SCML_USE_SDL2)
    if (g_builtin.window) SDL_SetWindowTitle(g_builtin.window, title);
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_window_poll_events(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { return rt_input_poll_events(vm, args, arg_count, ret, user_data); }
static int rt_audio_load_sound(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(++g_builtin.next_handle); return 1; }
static int rt_audio_stop_sound(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(0); return 1; }
static int rt_audio_set_volume(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 1) return 0; g_builtin.audio_volume = args[0].integer; *ret = scml_value_int(g_builtin.audio_volume); return 1; }
static int rt_gpu_noop(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; if (!g_builtin.gpu_context_alive) return 0; *ret = scml_value_int(0); return 1; }
static int rt_entity_set_noop(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; *ret = scml_value_int(0); return 1; }
static int rt_gpu_create_buffer(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    if (!g_builtin.gpu_context_alive) return 0;
#if defined(SCML_USE_OPENGL)
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    if (!vbo) return 0;
    *ret = scml_value_int((int32_t)vbo);
    return 1;
#else
    return rt_create_handle(vm, args, arg_count, ret, user_data);
#endif
}
static int rt_gpu_update_buffer(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.gpu_context_alive || arg_count < 2) return 0;
#if !defined(SCML_USE_OPENGL)
    (void)args;
#endif
#if defined(SCML_USE_OPENGL)
    GLuint vbo = (GLuint)args[0].integer;
    if (!vbo) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    g_builtin.active_vbo = vbo;
    if (args[1].type == SCML_VAL_STRING && args[1].string) {
        size_t n = strlen(args[1].string);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n, args[1].string, GL_DYNAMIC_DRAW);
    }
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_gpu_draw_triangle(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    if (!g_builtin.gpu_context_alive) return 0;
#if defined(SCML_USE_OPENGL)
    glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_gpu_draw_indexed(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.gpu_context_alive) return 0;
    int count = arg_count > 0 ? args[0].integer : 0;
#if !defined(SCML_USE_OPENGL)
    (void)count;
#endif
#if defined(SCML_USE_OPENGL)
    if (count > 0) glDrawArrays(GL_TRIANGLES, 0, count);
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_gpu_draw_mesh(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    return rt_gpu_draw_indexed(vm, args, arg_count, ret, user_data);
}
static int rt_gpu_clear_color(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (!g_builtin.gpu_context_alive) return 0;
    float r = arg_count > 0 ? (args[0].type == SCML_VAL_FLOAT ? args[0].real : (float)args[0].integer) : 0.0f;
    float g = arg_count > 1 ? (args[1].type == SCML_VAL_FLOAT ? args[1].real : (float)args[1].integer) : 0.0f;
    float b = arg_count > 2 ? (args[2].type == SCML_VAL_FLOAT ? args[2].real : (float)args[2].integer) : 0.0f;
    float a = arg_count > 3 ? (args[3].type == SCML_VAL_FLOAT ? args[3].real : (float)args[3].integer) : 1.0f;
#if !(defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES))
    (void)r; (void)g; (void)b; (void)a;
#endif
#if defined(SCML_USE_OPENGL) || defined(SCML_USE_OPENGLES)
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    *ret = scml_value_int(0);
    return 1;
}
static int rt_file_copy_file(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)user_data; if (arg_count < 2) return 0; char s[512], d[512]; const char *src=arg_to_cstr(&args[0], s, sizeof(s)); const char *dst=arg_to_cstr(&args[1], d, sizeof(d)); FILE *fs=fopen(src,"rb"); if(!fs) return 0; FILE *fd=fopen(dst,"wb"); if(!fd){fclose(fs); return 0;} char buf[4096]; size_t n; while((n=fread(buf,1,sizeof(buf),fs))>0) fwrite(buf,1,n,fd); fclose(fs); fclose(fd); *ret=scml_value_int(0); return 1; }
static int rt_file_list_directory(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char p[512];
    const char *path = arg_to_cstr(&args[0], p, sizeof(p));
    char out[1024] = {0};
#if defined(_WIN32)
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s%s*", path, (path[0] && path[strlen(path) - 1] != '\\' && path[strlen(path) - 1] != '/') ? "\\" : "");
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA(pattern, &data);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, "..")) {
            if (out[0]) strncat(out, ",", sizeof(out) - strlen(out) - 1);
            strncat(out, data.cFileName, sizeof(out) - strlen(out) - 1);
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
#else
    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            if (out[0]) strncat(out, ",", sizeof(out) - strlen(out) - 1);
            strncat(out, e->d_name, sizeof(out) - strlen(out) - 1);
        }
    }
    closedir(d);
#endif
    *ret = scml_value_string(out);
    return 1;
}
static int rt_system_get_platform(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    *ret = scml_value_string("windows");
#elif defined(__APPLE__)
    *ret = scml_value_string("macos");
#elif defined(__linux__)
    *ret = scml_value_string("linux");
#else
    *ret = scml_value_string("unknown");
#endif
    return 1; }
static int rt_system_get_cpu_count(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    *ret = scml_value_int((int32_t)info.dwNumberOfProcessors);
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    *ret = scml_value_int((int32_t)(count > 0 ? count : 1));
#endif
    return 1;
}

static int rt_system_get_memory_info(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
#if defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return 0;
    *ret = scml_value_int((int32_t)(status.ullTotalPhys / (1024ULL * 1024ULL)));
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || page_size <= 0) return 0;
    long long bytes = (long long)pages * (long long)page_size;
    *ret = scml_value_int((int32_t)(bytes / (1024 * 1024)));
#endif
    return 1;
}

static int rt_system_get_working_directory(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    char buf[512];
#if defined(_WIN32)
    DWORD n = GetCurrentDirectoryA((DWORD)sizeof(buf), buf);
    if (n == 0 || n >= sizeof(buf)) return 0;
#else
    if (!getcwd(buf, sizeof(buf))) return 0;
#endif
    *ret = scml_value_string(buf);
    return 1;
}

typedef struct ThreadSleepJob { int slot; int ms; } ThreadSleepJob;

#if !defined(_WIN32)
static void *rt_thread_sleep_main(void *arg) {
    ThreadSleepJob *job = (ThreadSleepJob *)arg;
    int slot = job->slot;
    int ms = job->ms;
    free(job);
    if (ms < 0) ms = 0;
    scml_runtime_sleep_us((unsigned int)ms * 1000u);
    g_builtin.thread_done[slot] = 1;
    return NULL;
}
#endif

static int rt_thread_create(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    int ms = arg_count > 0 ? args[0].integer : 0;
    for (int i = 0; i < 64; i++) {
        if (!g_builtin.thread_active[i]) {
            g_builtin.thread_active[i] = 1;
            g_builtin.thread_done[i] = 0;
#if defined(_WIN32)
            Sleep((DWORD)(ms < 0 ? 0 : ms));
            g_builtin.thread_done[i] = 1;
#else
            ThreadSleepJob *job = (ThreadSleepJob *)malloc(sizeof(*job));
            if (!job) { g_builtin.thread_active[i] = 0; return 0; }
            job->slot = i;
            job->ms = ms;
            if (pthread_create(&g_builtin.thread_handles[i], NULL, rt_thread_sleep_main, job) != 0) {
                free(job);
                g_builtin.thread_active[i] = 0;
                return 0;
            }
#endif
            *ret = scml_value_int(i + 1);
            return 1;
        }
    }
    return 0;
}

static int rt_thread_join(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    int slot = args[0].integer - 1;
    if (slot < 0 || slot >= 64 || !g_builtin.thread_active[slot]) return 0;
#if !defined(_WIN32)
    pthread_join(g_builtin.thread_handles[slot], NULL);
#endif
    g_builtin.thread_done[slot] = 1;
    g_builtin.thread_active[slot] = 0;
    *ret = scml_value_int(0);
    return 1;
}

static int rt_thread_done(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    int slot = args[0].integer - 1;
    *ret = scml_value_int(slot >= 0 && slot < 64 && g_builtin.thread_active[slot] && g_builtin.thread_done[slot]);
    return 1;
}

static int rt_thread_yield(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) { (void)vm; (void)args; (void)arg_count; (void)user_data; scml_runtime_sleep_us(0); *ret = scml_value_int(0); return 1; }

static int rt_runtime_sleep_ms(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    int ms = args[0].type == SCML_VAL_INT ? args[0].integer : 0;
#if defined(_WIN32)
    Sleep((DWORD)(ms < 0 ? 0 : ms));
    *ret = scml_value_int(0);
    return 1;
#else
    if (ms < 0) ms = 0;
    scml_runtime_sleep_us((unsigned int)ms * 1000u);
    *ret = scml_value_int(0);
    return 1;
#endif
}


static int rt_console_write(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    if (arg_count < 1) return 0;
    char buf[128];
    fputs(arg_to_cstr(&args[0], buf, sizeof(buf)), stdout);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_flush(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_clear(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_home(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    fputs("\033[H", stdout);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int runtime_checked_int_arg(const ScmlValue *args, size_t arg_count, size_t index, int min_value, int max_value, int *out) {
    if (index >= arg_count) return 0;
    int value = args[index].type == SCML_VAL_FLOAT ? (int)args[index].real : (args[index].type == SCML_VAL_INT ? args[index].integer : atoi(args[index].string ? args[index].string : "0"));
    if (value < min_value || value > max_value) return 0;
    *out = value;
    return 1;
}

static int rt_console_color(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    int fg = 0, bg = -1;
    if (!runtime_checked_int_arg(args, arg_count, 0, 0, 255, &fg)) return 0;
    if (arg_count > 1 && !runtime_checked_int_arg(args, arg_count, 1, 0, 255, &bg)) return 0;
    if (bg >= 0) fprintf(stdout, "\033[38;5;%dm\033[48;5;%dm", fg, bg);
    else fprintf(stdout, "\033[38;5;%dm", fg);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_reset(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    fputs("\033[0m", stdout);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_move(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    int row = 0, col = 0;
    if (!runtime_checked_int_arg(args, arg_count, 0, 1, 9999, &row) ||
        !runtime_checked_int_arg(args, arg_count, 1, 1, 9999, &col)) return 0;
    fprintf(stdout, "\033[%d;%dH", row, col);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_erase_line(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    fputs("\033[2K", stdout);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_console_style(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)user_data;
    int style = 0;
    if (!runtime_checked_int_arg(args, arg_count, 0, 0, 9, &style)) return 0;
    fprintf(stdout, "\033[%dm", style);
    fflush(stdout);
    *ret = scml_value_int(0);
    return 1;
}

static int rt_runtime_select_backend(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)user_data;
    if (arg_count < 2) return 0;
    char mbuf[64], bbuf[64];
    const char *module_name = arg_to_cstr(&args[0], mbuf, sizeof(mbuf));
    const char *backend_name = arg_to_cstr(&args[1], bbuf, sizeof(bbuf));
    *ret = scml_value_int(scml_runtime_select_backend(vm, module_name, backend_name) ? 1 : 0);
    return 1;
}

static int rt_capability_info(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm;
    char mbuf[64];
    const char *module_name = (const char *)user_data;
    if (arg_count > 0) module_name = arg_to_cstr(&args[0], mbuf, sizeof(mbuf));
    if (!module_name || !*module_name) module_name = "runtime";
    char buf[1024] = {0};
    if (!scml_runtime_backend_info(buf, sizeof(buf), module_name)) {
        scml_runtime_append_compile_flags(buf, sizeof(buf));
    }
    *ret = scml_value_string(buf);
    return 1;
}

/* Default extensible surfaces:
 * - OpenGL/OpenGL ES/Vulkan backends map to gpu.* functions
 * - SDL Audio/OpenAL/WASAPI/ALSA backends map to audio.* functions
 * - TXT/Imagen helpers are runtime-side optional APIs exposed via file/image modules
 * Backends can run with built-in headless behavior and can be replaced by platform backends.
 * that call CALL_NATIVE module.function entry points.
 */
static const ScmlRuntimeFunctionEntry k_gpu[] = {
    {"create_context", rt_gpu_create_context}, {"destroy_context", rt_gpu_destroy_context}, {"begin_frame", rt_gpu_begin_frame}, {"end_frame", rt_gpu_end_frame}, {"present", rt_gpu_present},
    {"clear_color", rt_gpu_clear_color}, {"set_viewport", rt_gpu_noop}, {"create_buffer", rt_gpu_create_buffer}, {"update_buffer", rt_gpu_update_buffer}, {"destroy_buffer", rt_gpu_noop},
    {"create_texture2d", rt_gpu_create_texture2d}, {"update_texture2d", rt_gpu_update_texture2d}, {"destroy_texture", rt_gpu_destroy_texture}, {"create_shader", rt_create_handle}, {"set_shader", rt_gpu_noop},
    {"set_uniform", rt_gpu_noop}, {"draw_triangle", rt_gpu_draw_triangle}, {"draw_indexed", rt_gpu_draw_indexed}, {"draw_mesh", rt_gpu_draw_mesh},
    {"create_window", rt_window_create}, {"load_texture", rt_create_handle}, {"load_image", rt_create_handle}, {"upload_image", rt_gpu_noop}, {"create_pipeline", rt_create_handle},
    {"entity_spawn", rt_create_handle}, {"entity_set", rt_entity_set_noop},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_audio[] = {
    {"load_sound", rt_audio_load_sound}, {"play_sound", rt_audio_play_sound}, {"stop_sound", rt_audio_stop_sound}, {"set_volume", rt_audio_set_volume},
    {"play_music", rt_audio_play_sound}, {"pause_music", rt_audio_stop_sound}, {"resume_music", rt_audio_play_sound},
    {"stream_audio", rt_audio_play_sound}, {"set_listener", rt_input_poll_events}, {"set_3d_position", rt_input_poll_events}, {"set_panning", rt_input_poll_events},
    {"play", rt_audio_play_sound}, {"stop", rt_audio_stop_sound},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_file[] = {
    {"exists", rt_file_exists}, {"read_txt", rt_file_read_txt}, {"write_txt", rt_file_write_txt}, {"copy_file", rt_file_copy_file},
    {"list_directory", rt_file_list_directory}, {"delete_file", rt_file_delete_file}, {"create_directory", rt_file_create_directory},
    {"open_file", rt_create_handle}, {"read_file", rt_file_read_txt}, {"write_file", rt_file_write_txt}, {"open", rt_create_handle}, {"read", rt_file_read_txt}, {"write", rt_file_write_txt},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_image[] = {
    {"load_image", rt_create_handle}, {"save_image", rt_file_write_txt}, {"decode_png", rt_create_handle}, {"decode_jpg", rt_create_handle}, {"encode_png", rt_file_write_txt},
    {"resize_image", rt_input_poll_events}, {"blit_image", rt_input_poll_events}, {"get_image_info", rt_input_get_mouse_position},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_net[] = {
    {"open_socket", rt_net_open_socket}, {"close_socket", rt_net_close_socket}, {"connect", rt_net_connect}, {"send_data", rt_net_send_data},
    {"receive_data", rt_net_receive_data}, {"request", rt_net_request}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_input[] = {
    {"poll_events", rt_input_poll_events}, {"is_key_down", rt_input_is_key_down}, {"is_key_up", rt_input_is_key_up}, {"get_mouse_position", rt_input_get_mouse_position},
    {"is_mouse_button_down", rt_input_is_mouse_button_down}, {"is_mouse_button_up", rt_input_is_mouse_button_up}, {"get_mouse_wheel", rt_input_get_mouse_wheel},
    {"get_keyboard_state", rt_input_get_mouse_position}, {"read", rt_input_poll_events}, {"isKeyDown", rt_input_is_key_down},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_window[] = {
    {"create", rt_window_create}, {"destroy", rt_window_destroy}, {"show", rt_window_show}, {"hide", rt_window_hide}, {"resize", rt_window_resize},
    {"set_title", rt_window_set_title}, {"poll_events", rt_window_poll_events}, {"close_requested", rt_window_close_requested}, {"should_close", rt_window_close_requested},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_console[] = {
    {"write", rt_console_write}, {"flush", rt_console_flush}, {"clear", rt_console_clear},
    {"home", rt_console_home}, {"color", rt_console_color}, {"reset", rt_console_reset},
    {"move", rt_console_move}, {"erase_line", rt_console_erase_line}, {"style", rt_console_style}
};
static const ScmlRuntimeFunctionEntry k_runtime[] = {
    {"wait", rt_runtime_sleep_ms}, {"get_time_ms", rt_runtime_get_time_ms}, {"get_time_us", rt_runtime_get_time_us}, {"get_ticks", rt_runtime_get_ticks},
    {"select_backend", rt_runtime_select_backend}, {"backend_info", rt_capability_info}, {"capability_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_data[] = {
    {"hash32", rt_data_hash32}, {"url_encode", rt_data_url_encode}, {"url_decode", rt_data_url_decode},
    {"split", rt_data_split}, {"json_get", rt_data_json_get}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_env[] = {
    {"get", rt_env_get}, {"set", rt_env_set}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_system[] = {
    {"get_platform", rt_system_get_platform}, {"get_cpu_count", rt_system_get_cpu_count}, {"get_memory_info", rt_system_get_memory_info},
    {"get_working_directory", rt_system_get_working_directory}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_thread[] = {
    {"create", rt_thread_create}, {"join", rt_thread_join}, {"done", rt_thread_done}, {"sleep", rt_runtime_sleep_ms}, {"yield", rt_thread_yield}
};


int scml_runtime_install_builtin_module_registry(ScmlVM *vm) {
    int ok = 1;
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "default", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#if defined(SCML_USE_OPENGL)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "opengl", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif
#if defined(SCML_USE_OPENGLES)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "opengles", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif
#if defined(SCML_USE_VULKAN)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "vulkan", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif
#if defined(SCML_USE_D3D11)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "directx11", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif
#if defined(SCML_USE_D3D12)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "directx12", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif
#if defined(SCML_USE_METAL)
    ok = ok && scml_runtime_register_builtin_backend(vm, "gpu", "metal", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]));
#endif

    ok = ok && scml_runtime_register_builtin_backend(vm, "audio", "default", k_audio, sizeof(k_audio) / sizeof(k_audio[0]));
#if defined(SCML_USE_SDL2)
    ok = ok && scml_runtime_register_builtin_backend(vm, "audio", "sdl_audio", k_audio, sizeof(k_audio) / sizeof(k_audio[0]));
#endif
#if defined(_WIN32)
    ok = ok && scml_runtime_register_builtin_backend(vm, "audio", "wasapi", k_audio, sizeof(k_audio) / sizeof(k_audio[0]));
#endif
#if defined(__linux__)
    ok = ok && scml_runtime_register_builtin_backend(vm, "audio", "alsa", k_audio, sizeof(k_audio) / sizeof(k_audio[0]));
#endif

    ok = ok && scml_runtime_register_builtin_backend(vm, "file", "default", k_file, sizeof(k_file) / sizeof(k_file[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "image", "default", k_image, sizeof(k_image) / sizeof(k_image[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "net", "default", k_net, sizeof(k_net) / sizeof(k_net[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "input", "default", k_input, sizeof(k_input) / sizeof(k_input[0]));
#if defined(SCML_USE_SDL2)
    ok = ok && scml_runtime_register_builtin_backend(vm, "input", "sdl2", k_input, sizeof(k_input) / sizeof(k_input[0]));
#endif
    ok = ok && scml_runtime_register_builtin_backend(vm, "console", "default", k_console, sizeof(k_console) / sizeof(k_console[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "runtime", "default", k_runtime, sizeof(k_runtime) / sizeof(k_runtime[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "data", "default", k_data, sizeof(k_data) / sizeof(k_data[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "env", "default", k_env, sizeof(k_env) / sizeof(k_env[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "system", "default", k_system, sizeof(k_system) / sizeof(k_system[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "thread", "default", k_thread, sizeof(k_thread) / sizeof(k_thread[0]));
    ok = ok && scml_runtime_register_builtin_backend(vm, "window", "default", k_window, sizeof(k_window) / sizeof(k_window[0]));
#if defined(SCML_USE_SDL2)
    ok = ok && scml_runtime_register_builtin_backend(vm, "window", "sdl2", k_window, sizeof(k_window) / sizeof(k_window[0]));
#endif
#if defined(__linux__)
    ok = ok && scml_runtime_register_builtin_backend(vm, "window", "x11", k_window, sizeof(k_window) / sizeof(k_window[0]));
#endif
#if defined(_WIN32)
    ok = ok && scml_runtime_register_builtin_backend(vm, "window", "win32", k_window, sizeof(k_window) / sizeof(k_window[0]));
#endif
    ok = ok && scml_runtime_select_backend(vm, "gpu", "default");
    ok = ok && scml_runtime_select_backend(vm, "audio", "default");
    ok = ok && scml_runtime_select_backend(vm, "file", "default");
    ok = ok && scml_runtime_select_backend(vm, "image", "default");
    ok = ok && scml_runtime_select_backend(vm, "net", "default");
    ok = ok && scml_runtime_select_backend(vm, "input", "default");
    ok = ok && scml_runtime_select_backend(vm, "console", "default");
    ok = ok && scml_runtime_select_backend(vm, "runtime", "default");
    ok = ok && scml_runtime_select_backend(vm, "data", "default");
    ok = ok && scml_runtime_select_backend(vm, "env", "default");
    ok = ok && scml_runtime_select_backend(vm, "system", "default");
    ok = ok && scml_runtime_select_backend(vm, "thread", "default");
    ok = ok && scml_runtime_select_backend(vm, "window", "default");
    return ok;
}
