#include "compiler/compiler.h"
#include "vm/vm.h"
#include <stdio.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr, "usage:\n  %s compile input.scml output.scmlbin\n  %s run input.scmlbin [--trace]\n", argv0, argv0);
}

int main(int argc, char **argv) {
    char err[1024] = {0};
    if (argc < 2) { usage(argv[0]); return 1; }
    if (strcmp(argv[1], "compile") == 0) {
        if (argc < 4) { usage(argv[0]); return 1; }
        if (!scml_compile_file(argv[2], argv[3], err, sizeof(err))) { fprintf(stderr, "compile error: %s\n", err); return 1; }
        return 0;
    }
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        ScmlVM *vm = scml_vm_create();
        if (!vm) { fprintf(stderr, "out of memory\n"); return 1; }
        if (argc > 3 && strcmp(argv[3], "--trace") == 0) scml_vm_set_trace(vm, 1);
        if (!scml_vm_load_file(vm, argv[2], err, sizeof(err)) || !scml_vm_run(vm, err, sizeof(err))) {
            fprintf(stderr, "runtime error: %s\n", err);
            scml_vm_destroy(vm);
            return 1;
        }
        scml_vm_destroy(vm);
        return 0;
    }
    usage(argv[0]);
    return 1;
}
