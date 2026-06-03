cmake_minimum_required(VERSION 3.16)
project(OpenSCM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCML_BUILD_EMBED_EXAMPLES "Build C++ embedding examples" ON)
option(SCML_BUILD_EDITOR "Build the lightweight SCML editor sample" ON)
option(SCML_AUTODETECT_BACKENDS "Detect and enable installed runtime backends such as SDL2 and OpenGL" ON)

set(SCML_CORE_SOURCES
    vm/vm.c
    compiler/compiler.c
    compiler/project.c
    parser/parser.c
    lexer/lexer.c
    opcode/opcode.c
    runtime/scml_runtime_modules.c
    ffi/scml_ffi.c
)

add_library(scml_core STATIC ${SCML_CORE_SOURCES})
target_include_directories(scml_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(scml_core PRIVATE /W4)
else()
    target_compile_options(scml_core PRIVATE -Wall -Wextra -pedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(scml_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBFFI QUIET libffi)
endif()
if(LIBFFI_FOUND)
    target_compile_definitions(scml_core PUBLIC SCML_USE_LIBFFI=1)
    target_include_directories(scml_core PUBLIC ${LIBFFI_INCLUDE_DIRS})
    target_link_libraries(scml_core PUBLIC ${LIBFFI_LIBRARIES})
endif()

if(SCML_AUTODETECT_BACKENDS)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        target_compile_definitions(scml_core PUBLIC SCML_USE_SDL2=1)
        if(TARGET SDL2::SDL2)
            target_link_libraries(scml_core PUBLIC SDL2::SDL2)
        elseif(TARGET SDL2::SDL2-static)
            target_link_libraries(scml_core PUBLIC SDL2::SDL2-static)
        else()
            target_include_directories(scml_core PUBLIC ${SDL2_INCLUDE_DIRS})
            target_link_libraries(scml_core PUBLIC ${SDL2_LIBRARIES})
        endif()
    endif()

    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        target_compile_definitions(scml_core PUBLIC SCML_USE_OPENGL=1)
        if(TARGET OpenGL::GL)
            target_link_libraries(scml_core PUBLIC OpenGL::GL)
        else()
            target_include_directories(scml_core PUBLIC ${OPENGL_INCLUDE_DIR})
            target_link_libraries(scml_core PUBLIC ${OPENGL_LIBRARIES})
        endif()
    endif()

endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(scml_core PUBLIC m)
endif()

add_executable(scml main.c)
target_link_libraries(scml PRIVATE scml_core)

if(SCML_BUILD_EMBED_EXAMPLES)
    add_executable(scml_gameplay_example examples/gameplay_embed.cpp)
    target_link_libraries(scml_gameplay_example PRIVATE scml_core)
endif()

if(SCML_BUILD_EDITOR)
    add_executable(scml_editor editor/scml_editor.cpp debugger/debugger.c)
    target_link_libraries(scml_editor PRIVATE scml_core)
endif()
