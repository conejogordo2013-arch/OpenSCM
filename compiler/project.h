#ifndef SCML_PROJECT_H
#define SCML_PROJECT_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCML_PROJECT_MAX_SOURCES 8192
#define SCML_PROJECT_MAX_PACKAGES 1024
#define SCML_PROJECT_MAX_DEFINES 512
#define SCML_PROJECT_PATH_MAX 512

typedef struct ScmlProject {
    char name[128];
    char root[SCML_PROJECT_PATH_MAX];
    char output[SCML_PROJECT_PATH_MAX];
    char sources[SCML_PROJECT_MAX_SOURCES][SCML_PROJECT_PATH_MAX];
    size_t source_count;
    char packages[SCML_PROJECT_MAX_PACKAGES][SCML_PROJECT_PATH_MAX];
    size_t package_count;
    char defines[SCML_PROJECT_MAX_DEFINES][256];
    size_t define_count;
    int jobs;
} ScmlProject;

int scml_project_load(const char *manifest_path, ScmlProject *project, char *err, size_t err_size);
int scml_project_compile(const char *manifest_path, char *err, size_t err_size);
int scml_project_check(const char *manifest_path, char *err, size_t err_size);
int scml_project_init(const char *dir, const char *name, char *err, size_t err_size);
int scml_project_metadata(const char *manifest_path, FILE *out, char *err, size_t err_size);
int scml_format_file(const char *path, char *err, size_t err_size);

#ifdef __cplusplus
}
#endif

#endif
