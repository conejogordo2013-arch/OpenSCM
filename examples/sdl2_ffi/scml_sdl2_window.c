/*
 * scml_sdl2_window.c - tiny SDL2 helper for SCML FFI examples.
 *
 * Build on Linux:
 *   cc -shared -fPIC -o examples/sdl2_ffi/libscml_sdl2_window.so \
 *      examples/sdl2_ffi/scml_sdl2_window.c $(pkg-config --cflags --libs sdl2)
 *
 * Build on MSYS2 UCRT64:
 *   gcc -shared -o examples/sdl2_ffi/scml_sdl2_window.dll \
 *      examples/sdl2_ffi/scml_sdl2_window.c $(pkg-config --cflags --libs sdl2)
 */
#include <stdint.h>

#include <SDL2/SDL.h>

#if defined(_WIN32)
#define SCML_SDL2_EXPORT __declspec(dllexport)
#else
#define SCML_SDL2_EXPORT __attribute__((visibility("default")))
#endif

SCML_SDL2_EXPORT int32_t scml_sdl2_open_window_ms(int32_t width, int32_t height, const char *title, int32_t milliseconds) {
    if (width <= 0 || height <= 0) return -1;
    if (milliseconds < 0) milliseconds = 0;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return -2;

    SDL_Window *window = SDL_CreateWindow(title ? title : "SCML SDL2 FFI",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          width,
                                          height,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return -3;
    }

    int32_t elapsed = 0;
    while (elapsed < milliseconds) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) elapsed = milliseconds;
        }
        SDL_Delay(16);
        elapsed += 16;
    }

    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
}
