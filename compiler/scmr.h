#ifndef SCML_SCMR_H
#define SCML_SCMR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int scml_build_scmr(const char *scmlbin_path,
                    size_t asset_count,
                    const char **asset_paths,
                    const char *output_path,
                    char *err,
                    size_t err_size);

#ifdef __cplusplus
}
#endif

#endif
