#include "scml_runtime_modules.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
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

int scml_runtime_call_module_function(ScmlVM *vm, const char *qualified_name, const ScmlValue *args, size_t arg_count, ScmlValue *ret) {
    if (!vm || !qualified_name || !ret) return 0;
    return scml_vm_call_native(vm, qualified_name, args, arg_count, ret);
}

size_t scml_runtime_registered_module_count(void) {
    return g_registry.count;
}

static int null_ok(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    *ret = scml_value_int(0);
    return 1;
}

static const char *arg_to_cstr(const ScmlValue *v, char *buf, size_t n) {
    if (!v) return "";
    if (v->type == SCML_VAL_STRING) return v->string ? v->string : "";
    if (v->type == SCML_VAL_FLOAT) { snprintf(buf, n, "%g", v->real); return buf; }
    snprintf(buf, n, "%d", v->integer);
    return buf;
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
    *ret = scml_value_int(0);
    return 1;
#else
    *ret = scml_value_int(-1);
    return 0;
#endif
}

static int rt_net_open_socket(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    *ret = scml_value_int(1);
    return 1;
}

static int rt_net_send_data(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    *ret = scml_value_int((int32_t)(arg_count > 1 ? 1 : 0));
    return arg_count > 1;
}

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
    sleep((unsigned int)((ms + 999) / 1000));
    *ret = scml_value_int(0);
    return 1;
#endif
}

static int rt_capability_info(ScmlVM *vm, const ScmlValue *args, size_t arg_count, ScmlValue *ret, void *user_data) {
    (void)vm; (void)args; (void)arg_count; (void)user_data;
    char buf[512];
    snprintf(buf, sizeof(buf),
             "SDL2=%d OpenGL=%d OpenGLES=%d Vulkan=%d D3D11=%d D3D12=%d Metal=%d",
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
    *ret = scml_value_string(buf);
    return 1;
}

/* Default extensible surfaces:
 * - OpenGL/OpenGL ES/Vulkan backends map to gpu.* functions
 * - SDL Audio/OpenAL/WASAPI/ALSA backends map to audio.* functions
 * - TXT/Imagen helpers are runtime-side optional APIs exposed via file/image modules
 * Backends are placeholders by default and become active only through SCML libraries
 * that call CALL_NATIVE module.function entry points.
 */
static const ScmlRuntimeFunctionEntry k_gpu[] = {
    {"create_window", null_ok}, {"begin_frame", null_ok}, {"draw_triangle", null_ok}, {"draw_mesh", null_ok}, {"present", null_ok},
    {"load_texture", null_ok}, {"load_image", null_ok}, {"upload_image", null_ok}, {"set_viewport", null_ok}, {"clear_color", null_ok},
    {"entity_spawn", null_ok},
    {"create_pipeline", null_ok}, {"set_shader", null_ok}, {"set_uniform", null_ok}, {"draw_indexed", null_ok},
    {"create_buffer", null_ok}, {"update_buffer", null_ok}, {"destroy_buffer", null_ok},
    {"create_texture2d", null_ok}, {"update_texture2d", null_ok}, {"destroy_texture", null_ok},
    {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_audio[] = {
    {"play_sound", rt_audio_play_sound}, {"stop_sound", null_ok}, {"set_volume", null_ok}, {"stream_audio", null_ok},
    {"load_sound", null_ok}, {"play_music", null_ok}, {"pause_music", null_ok}, {"resume_music", null_ok},
    {"set_listener", null_ok}, {"set_3d_position", null_ok}, {"set_panning", null_ok},
    {"play", null_ok}, {"stop", null_ok}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_file[] = {
    {"open_file", null_ok}, {"read_file", null_ok}, {"write_file", null_ok}, {"list_directory", null_ok},
    {"read_txt", rt_file_read_txt}, {"write_txt", rt_file_write_txt}, {"exists", null_ok}, {"copy_file", null_ok},
    {"open", null_ok}, {"read", null_ok}, {"write", null_ok}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_image[] = {
    {"load_image", null_ok}, {"save_image", null_ok}, {"resize_image", null_ok}, {"blit_image", null_ok}, {"get_image_info", null_ok},
    {"decode_png", null_ok}, {"decode_jpg", null_ok}, {"encode_png", null_ok}, {"backend_info", rt_capability_info}
};
static const ScmlRuntimeFunctionEntry k_net[] = {{"open_socket", rt_net_open_socket}, {"send_data", rt_net_send_data}, {"receive_data", null_ok}, {"connect", null_ok}, {"request", null_ok}, {"backend_info", rt_capability_info}};
static const ScmlRuntimeFunctionEntry k_input[] = {{"get_keyboard_state", null_ok}, {"get_mouse_position", null_ok}, {"poll_events", null_ok}, {"read", null_ok}, {"isKeyDown", null_ok}, {"backend_info", rt_capability_info}};
static const ScmlRuntimeFunctionEntry k_window[] = {{"create", null_ok}, {"show", null_ok}, {"resize", null_ok}, {"set_title", null_ok}, {"backend_info", rt_capability_info}};
static const ScmlRuntimeFunctionEntry k_runtime[] = {{"wait", rt_runtime_sleep_ms}};

int scml_runtime_install_builtin_module_registry(ScmlVM *vm) {
    int ok = 1;
    ScmlRuntimeBackendVTable gpu_default = {"default", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable gpu_gl = {"opengl", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable gpu_vk = {"vulkan", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable gpu_dx = {"directx12", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable gpu_metal = {"metal", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable gpu_gles = {"opengles", k_gpu, sizeof(k_gpu) / sizeof(k_gpu[0]), NULL};
    ScmlRuntimeBackendVTable audio_default = {"default", k_audio, sizeof(k_audio) / sizeof(k_audio[0]), NULL};
    ScmlRuntimeBackendVTable audio_sdl = {"sdl_audio", k_audio, sizeof(k_audio) / sizeof(k_audio[0]), NULL};
    ScmlRuntimeBackendVTable audio_openal = {"openal", k_audio, sizeof(k_audio) / sizeof(k_audio[0]), NULL};
    ScmlRuntimeBackendVTable audio_wasapi = {"wasapi", k_audio, sizeof(k_audio) / sizeof(k_audio[0]), NULL};
    ScmlRuntimeBackendVTable audio_alsa = {"alsa", k_audio, sizeof(k_audio) / sizeof(k_audio[0]), NULL};
    ScmlRuntimeBackendVTable file_default = {"default", k_file, sizeof(k_file) / sizeof(k_file[0]), NULL};
    ScmlRuntimeBackendVTable image_default = {"default", k_image, sizeof(k_image) / sizeof(k_image[0]), NULL};
    ScmlRuntimeBackendVTable image_stb = {"stb_image", k_image, sizeof(k_image) / sizeof(k_image[0]), NULL};
    ScmlRuntimeBackendVTable net_default = {"default", k_net, sizeof(k_net) / sizeof(k_net[0]), NULL};
    ScmlRuntimeBackendVTable input_default = {"default", k_input, sizeof(k_input) / sizeof(k_input[0]), NULL};
    ScmlRuntimeBackendVTable runtime_default = {"default", k_runtime, sizeof(k_runtime) / sizeof(k_runtime[0]), NULL};
    ScmlRuntimeBackendVTable window_default = {"default", k_window, sizeof(k_window) / sizeof(k_window[0]), NULL};
    ScmlRuntimeBackendVTable window_sdl = {"sdl2", k_window, sizeof(k_window) / sizeof(k_window[0]), NULL};
    ScmlRuntimeBackendVTable window_x11 = {"x11", k_window, sizeof(k_window) / sizeof(k_window[0]), NULL};
    ScmlRuntimeBackendVTable window_win32 = {"win32", k_window, sizeof(k_window) / sizeof(k_window[0]), NULL};
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_default);
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_gl);
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_vk);
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_dx);
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_metal);
    ok = ok && scml_runtime_register_backend(vm, "gpu", &gpu_gles);
    ok = ok && scml_runtime_register_backend(vm, "audio", &audio_default);
    ok = ok && scml_runtime_register_backend(vm, "audio", &audio_sdl);
    ok = ok && scml_runtime_register_backend(vm, "audio", &audio_openal);
    ok = ok && scml_runtime_register_backend(vm, "audio", &audio_wasapi);
    ok = ok && scml_runtime_register_backend(vm, "audio", &audio_alsa);
    ok = ok && scml_runtime_register_backend(vm, "file", &file_default);
    ok = ok && scml_runtime_register_backend(vm, "image", &image_default);
    ok = ok && scml_runtime_register_backend(vm, "image", &image_stb);
    ok = ok && scml_runtime_register_backend(vm, "net", &net_default);
    ok = ok && scml_runtime_register_backend(vm, "input", &input_default);
    ok = ok && scml_runtime_register_backend(vm, "runtime", &runtime_default);
    ok = ok && scml_runtime_register_backend(vm, "window", &window_default);
    ok = ok && scml_runtime_register_backend(vm, "window", &window_sdl);
    ok = ok && scml_runtime_register_backend(vm, "window", &window_x11);
    ok = ok && scml_runtime_register_backend(vm, "window", &window_win32);
    ok = ok && scml_runtime_select_backend(vm, "gpu", "default");
    ok = ok && scml_runtime_select_backend(vm, "audio", "default");
    ok = ok && scml_runtime_select_backend(vm, "file", "default");
    ok = ok && scml_runtime_select_backend(vm, "image", "default");
    ok = ok && scml_runtime_select_backend(vm, "net", "default");
    ok = ok && scml_runtime_select_backend(vm, "input", "default");
    ok = ok && scml_runtime_select_backend(vm, "runtime", "default");
    ok = ok && scml_runtime_select_backend(vm, "window", "default");
    return ok;
}
