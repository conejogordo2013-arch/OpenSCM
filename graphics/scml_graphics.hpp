#ifndef SCML_GRAPHICS_HPP
#define SCML_GRAPHICS_HPP

#include "../runtime/game_runtime.hpp"

#include <iostream>

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>) && __has_include(<GL/gl.h>)
#    define SCML_HAS_SDL_OPENGL 1
#    include <SDL2/SDL.h>
#    include <GL/gl.h>
#  else
#    define SCML_HAS_SDL_OPENGL 0
#  endif
#else
#  define SCML_HAS_SDL_OPENGL 0
#endif

class SCML_Graphics {
public:
    bool init(const char *title, int width, int height) {
#if SCML_HAS_SDL_OPENGL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return false;
        window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
        if (!window_) return false;
        context_ = SDL_GL_CreateContext(window_);
        return context_ != nullptr;
#else
        (void)title; (void)width; (void)height;
        std::cout << "SCML graphics running in headless fallback mode (SDL2/OpenGL headers not found).\n";
        return true;
#endif
    }

    bool pollQuit() {
#if SCML_HAS_SDL_OPENGL
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) return true;
#endif
        return false;
    }

    void render(const SCML_GameRuntime &runtime) {
#if SCML_HAS_SDL_OPENGL
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glPointSize(10.0f);
        glBegin(GL_POINTS);
        for (const auto &entity : runtime.entities) {
            glVertex2f(entity.x / 100.0f, entity.y / 100.0f);
        }
        glEnd();
        SDL_GL_SwapWindow(window_);
#else
        std::cout << "entities=" << runtime.entities.size() << '\n';
#endif
    }

    void shutdown() {
#if SCML_HAS_SDL_OPENGL
        if (context_) SDL_GL_DeleteContext(context_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
#endif
    }

private:
#if SCML_HAS_SDL_OPENGL
    SDL_Window *window_ = nullptr;
    SDL_GLContext context_ = nullptr;
#endif
};

#endif
