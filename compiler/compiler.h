#ifndef SCML_COMPILER_H
#define SCML_COMPILER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" { 
#endif
int scml_compile_file(const char *source_path, const char *output_path, char *err, size_t err_size);
#ifdef __cplusplus
}
#endif
#endif
