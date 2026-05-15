#include "scmr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCMR_MAGIC 0x524D4353u /* 'SCMR' */
#define SCMR_VERSION 1u

static void w16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((v >> (i * 8)) & 255, f); }
static const char *base_name(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *m = a > b ? a : b;
    return m ? m + 1 : path;
}

static int file_size(const char *path, uint32_t *out, char *err, size_t err_size) {
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) { snprintf(err, err_size, "cannot open %s", path); return 0; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); snprintf(err, err_size, "seek failed %s", path); return 0; }
    n = ftell(f);
    fclose(f);
    if (n < 0 || (unsigned long)n > 0xFFFFFFFFul) { snprintf(err, err_size, "file too large %s", path); return 0; }
    *out = (uint32_t)n;
    return 1;
}

static int copy_file(FILE *out, const char *path, char *err, size_t err_size) {
    FILE *in = fopen(path, "rb");
    unsigned char buf[4096];
    size_t n;
    if (!in) { snprintf(err, err_size, "cannot open %s", path); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); snprintf(err, err_size, "write error"); return 0; }
    }
    fclose(in);
    return 1;
}

int scml_build_scmr(const char *scmlbin_path, size_t asset_count, const char **asset_paths, const char *output_path, char *err, size_t err_size) {
    FILE *f;
    uint32_t script_size;
    if (!file_size(scmlbin_path, &script_size, err, err_size)) return 0;
    f = fopen(output_path, "wb");
    if (!f) { snprintf(err, err_size, "cannot create %s", output_path); return 0; }

    w32(f, SCMR_MAGIC);
    w16(f, SCMR_VERSION);
    w16(f, 0);
    w32(f, (uint32_t)(asset_count + 1));

    {
        const char *name = "app.scmlbin";
        size_t len = strlen(name);
        w16(f, (uint16_t)len); fwrite(name, 1, len, f);
        w32(f, 1); w32(f, script_size);
    }

    for (size_t i = 0; i < asset_count; i++) {
        const char *name = base_name(asset_paths[i]);
        uint32_t sz;
        size_t len = strlen(name);
        if (len > 65535) { fclose(f); snprintf(err, err_size, "asset name too long"); return 0; }
        if (!file_size(asset_paths[i], &sz, err, err_size)) { fclose(f); return 0; }
        w16(f, (uint16_t)len); fwrite(name, 1, len, f);
        w32(f, 2); w32(f, sz);
    }

    if (!copy_file(f, scmlbin_path, err, err_size)) { fclose(f); remove(output_path); return 0; }
    for (size_t i = 0; i < asset_count; i++) {
        if (!copy_file(f, asset_paths[i], err, err_size)) { fclose(f); remove(output_path); return 0; }
    }

    fclose(f);
    return 1;
}
