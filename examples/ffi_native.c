#include <stdint.h>

#if defined(_WIN32)
#define SCML_FFI_EXPORT __declspec(dllexport)
#else
#define SCML_FFI_EXPORT __attribute__((visibility("default")))
#endif

SCML_FFI_EXPORT int32_t scml_ffi_add_i32(int32_t a, int32_t b) {
    return a + b;
}

SCML_FFI_EXPORT float scml_ffi_mul_f32(float a, float b) {
    return a * b;
}

SCML_FFI_EXPORT double scml_ffi_add_f64(double a, double b) {
    return a + b;
}

SCML_FFI_EXPORT int32_t scml_ffi_sum7_i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g) {
    return a + b + c + d + e + f + g;
}

SCML_FFI_EXPORT const char *scml_ffi_echo_string(const char *text) {
    return text;
}
