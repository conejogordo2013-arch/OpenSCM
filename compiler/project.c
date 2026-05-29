#define _POSIX_C_SOURCE 200809L
#include "project.h"
#include "compiler.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
extern int setenv(const char *, const char *, int);
extern int unsetenv(const char *);
#endif

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777)
#endif

static char *xstrdup2(const char *s) {
    size_t n = strlen(s ? s : "");
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s ? s : "", n + 1);
    return r;
}

static char *trim_ws(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int has_sep(const char *p) {
    size_t n = strlen(p);
    return n && (p[n - 1] == '/' || p[n - 1] == '\\');
}

static int is_abs_path(const char *p) {
    if (!p || !p[0]) return 0;
    if (p[0] == '/' || p[0] == '\\') return 1;
    return isalpha((unsigned char)p[0]) && p[1] == ':';
}

static void dirname_into(const char *path, char *out, size_t out_size) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *s = a > b ? a : b;
    if (!s) snprintf(out, out_size, ".");
    else if (s == path) snprintf(out, out_size, "%c", *s);
    else snprintf(out, out_size, "%.*s", (int)(s - path), path);
}

static void path_join(char *out, size_t out_size, const char *base, const char *rel) {
    if (is_abs_path(rel) || strcmp(base, ".") == 0) snprintf(out, out_size, "%s", rel);
    else snprintf(out, out_size, has_sep(base) ? "%s%s" : "%s/%s", base, rel);
}

static int ensure_dir(const char *path, char *err, size_t err_size) {
    if (!path || !path[0] || strcmp(path, ".") == 0) return 1;
    if (MKDIR(path) == 0 || errno == EEXIST) return 1;
    snprintf(err, err_size, "cannot create directory %s", path);
    return 0;
}

static int ensure_parent_dir(const char *path, char *err, size_t err_size) {
    char dir[SCML_PROJECT_PATH_MAX];
    dirname_into(path, dir, sizeof(dir));
    if (strcmp(dir, ".") == 0) return 1;
    return ensure_dir(dir, err, err_size);
}

static char *read_text(const char *path, char *err, size_t err_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, err_size, "cannot open %s", path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); snprintf(err, err_size, "cannot seek %s", path); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); snprintf(err, err_size, "cannot size %s", path); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); snprintf(err, err_size, "out of memory"); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); snprintf(err, err_size, "cannot read %s", path); return NULL; }
    buf[n] = 0;
    fclose(f);
    return buf;
}

static int strip_quotes(char *value) {
    size_t n = strlen(value);
    if (n >= 2 && ((value[0] == '"' && value[n - 1] == '"') || (value[0] == '\'' && value[n - 1] == '\''))) {
        memmove(value, value + 1, n - 2);
        value[n - 2] = 0;
    }
    return 1;
}

static int add_source(ScmlProject *p, const char *value, char *err, size_t err_size) {
    if (p->source_count >= SCML_PROJECT_MAX_SOURCES) { snprintf(err, err_size, "too many project sources"); return 0; }
    path_join(p->sources[p->source_count], sizeof(p->sources[p->source_count]), p->root, value);
    p->source_count++;
    return 1;
}

static int add_package(ScmlProject *p, const char *value, char *err, size_t err_size) {
    if (p->package_count >= SCML_PROJECT_MAX_PACKAGES) { snprintf(err, err_size, "too many project packages"); return 0; }
    path_join(p->packages[p->package_count], sizeof(p->packages[p->package_count]), p->root, value);
    p->package_count++;
    return 1;
}

int scml_project_load(const char *manifest_path, ScmlProject *project, char *err, size_t err_size) {
    memset(project, 0, sizeof(*project));
    snprintf(project->name, sizeof(project->name), "scml-project");
    dirname_into(manifest_path, project->root, sizeof(project->root));
    path_join(project->output, sizeof(project->output), project->root, "build/app.scmlbin");
    project->jobs = 1;

    char *text = read_text(manifest_path, err, err_size);
    if (!text) return 0;
    char *save = NULL;
    char *line = strtok_r(text, "\n", &save);
    int line_no = 1;
    while (line) {
        char *hash = strchr(line, '#');
        char *semi = strchr(line, ';');
        char *cut = hash && semi ? (hash < semi ? hash : semi) : (hash ? hash : semi);
        if (cut) *cut = 0;
        char *t = trim_ws(line);
        if (*t) {
            char *eq = strchr(t, '=');
            if (!eq) { snprintf(err, err_size, "%s:%d: expected key = value", manifest_path, line_no); free(text); return 0; }
            *eq = 0;
            char *key = trim_ws(t);
            char *value = trim_ws(eq + 1);
            strip_quotes(value);
            if (strcmp(key, "name") == 0) snprintf(project->name, sizeof(project->name), "%s", value);
            else if (strcmp(key, "output") == 0) path_join(project->output, sizeof(project->output), project->root, value);
            else if (strcmp(key, "source") == 0 || strcmp(key, "src") == 0) { if (!add_source(project, value, err, err_size)) { free(text); return 0; } }
            else if (strcmp(key, "package") == 0 || strcmp(key, "pkg") == 0) { if (!add_package(project, value, err, err_size)) { free(text); return 0; } }
            else if (strcmp(key, "jobs") == 0) { project->jobs = atoi(value); if (project->jobs < 1) project->jobs = 1; }
            else { snprintf(err, err_size, "%s:%d: unknown project key '%s'", manifest_path, line_no, key); free(text); return 0; }
        }
        line = strtok_r(NULL, "\n", &save);
        line_no++;
    }
    free(text);
    if (project->source_count == 0) { snprintf(err, err_size, "%s: project has no source entries", manifest_path); return 0; }
    return 1;
}

static int compile_loaded_project(const ScmlProject *project, const char *output_path, char *err, size_t err_size) {
    const char **sources = (const char **)calloc(project->source_count, sizeof(*sources));
    if (!sources) { snprintf(err, err_size, "out of memory"); return 0; }
    for (size_t i = 0; i < project->source_count; i++) sources[i] = project->sources[i];
    if (!ensure_parent_dir(output_path, err, err_size)) { free(sources); return 0; }

    char *old_path = xstrdup2(getenv("SCML_PATH"));
    char include_path[8192] = {0};
    strncat(include_path, project->root, sizeof(include_path) - strlen(include_path) - 1);
    strncat(include_path, ":.", sizeof(include_path) - strlen(include_path) - 1);
    for (size_t i = 0; i < project->package_count; i++) {
        if (include_path[0]) strncat(include_path, ":", sizeof(include_path) - strlen(include_path) - 1);
        strncat(include_path, project->packages[i], sizeof(include_path) - strlen(include_path) - 1);
    }
    if (old_path && old_path[0]) {
        if (include_path[0]) strncat(include_path, ":", sizeof(include_path) - strlen(include_path) - 1);
        strncat(include_path, old_path, sizeof(include_path) - strlen(include_path) - 1);
    }
#ifndef _WIN32
    if (include_path[0]) setenv("SCML_PATH", include_path, 1);
#endif
    int ok = scml_compile_files(project->source_count, sources, output_path, err, err_size);
#ifndef _WIN32
    if (old_path && old_path[0]) setenv("SCML_PATH", old_path, 1);
    else unsetenv("SCML_PATH");
#endif
    free(old_path);
    free(sources);
    return ok;
}

int scml_project_compile(const char *manifest_path, char *err, size_t err_size) {
    ScmlProject project;
    if (!scml_project_load(manifest_path, &project, err, err_size)) return 0;
    return compile_loaded_project(&project, project.output, err, err_size);
}

int scml_project_check(const char *manifest_path, char *err, size_t err_size) {
    ScmlProject project;
    if (!scml_project_load(manifest_path, &project, err, err_size)) return 0;
    char tmp[SCML_PROJECT_PATH_MAX];
    path_join(tmp, sizeof(tmp), project.root, ".scml/check.scmlbin");
    if (!ensure_parent_dir(tmp, err, err_size)) return 0;
    int ok = compile_loaded_project(&project, tmp, err, err_size);
    if (ok) remove(tmp);
    return ok;
}

int scml_project_init(const char *dir, const char *name, char *err, size_t err_size) {
    if (!ensure_dir(dir, err, err_size)) return 0;
    char srcdir[SCML_PROJECT_PATH_MAX];
    path_join(srcdir, sizeof(srcdir), dir, "src");
    if (!ensure_dir(srcdir, err, err_size)) return 0;
    char manifest[SCML_PROJECT_PATH_MAX];
    char main_src[SCML_PROJECT_PATH_MAX];
    path_join(manifest, sizeof(manifest), dir, "scml.pkg");
    path_join(main_src, sizeof(main_src), srcdir, "main.scml");
    FILE *mf = fopen(manifest, "wb");
    if (!mf) { snprintf(err, err_size, "cannot create %s", manifest); return 0; }
    fprintf(mf, "name = \"%s\"\nsource = \"src/main.scml\"\noutput = \"build/%s.scmlbin\"\njobs = 1\n", name && name[0] ? name : "app", name && name[0] ? name : "app");
    fclose(mf);
    FILE *sf = fopen(main_src, "wb");
    if (!sf) { snprintf(err, err_size, "cannot create %s", main_src); return 0; }
    fprintf(sf, ":MAIN\n03E5: \"hello from %s\"\n0001:\n", name && name[0] ? name : "SCML");
    fclose(sf);
    return 1;
}

int scml_format_file(const char *path, char *err, size_t err_size) {
    char *text = read_text(path, err, err_size);
    if (!text) return 0;
    FILE *out = fopen(path, "wb");
    if (!out) { snprintf(err, err_size, "cannot write %s", path); free(text); return 0; }
    char *save = NULL;
    char *line = strtok_r(text, "\n", &save);
    while (line) {
        char *t = trim_ws(line);
        if (*t == ':') fprintf(out, "%s\n", t);
        else if (*t) fprintf(out, "    %s\n", t);
        else fprintf(out, "\n");
        line = strtok_r(NULL, "\n", &save);
    }
    fclose(out);
    free(text);
    return 1;
}
