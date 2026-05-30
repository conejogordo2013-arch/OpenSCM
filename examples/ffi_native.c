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

SCML_FFI_EXPORT int32_t scml_ffi_sum15_i32(int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8, int32_t a9, int32_t a10, int32_t a11, int32_t a12, int32_t a13, int32_t a14, int32_t a15) {
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15;
}

SCML_FFI_EXPORT const char *scml_ffi_echo_string(const char *text) {
    return text;
}
