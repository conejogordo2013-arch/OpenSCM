#include "compiler/compiler.h"
#include "runtime/scml_runtime_modules.h"
#include "vm/vm.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s compile input.scml output.scmlbin\n"
            "  %s compile input1.scml input2.scml output.scmlbin\n"
            "  %s run input.scmlbin [--trace] [--dump-memory] [--no-builtin-modules] [--trigger EVENT]\n",
            argv0, argv0, argv0);
}

int main(int argc, char **argv) {
    char err[1024] = {0};
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "compile") == 0) {
        if (argc < 4) { usage(argv[0]); return 1; }
        const char *out = argv[argc - 1];
        const char **inputs = (const char **)&argv[2];
        size_t input_count = (size_t)(argc - 3);
        if (!scml_compile_files(input_count, inputs, out, err, sizeof(err))) {
            fprintf(stderr, "compile error: %s\n", err);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        ScmlVM *vm = scml_vm_create();
        int dump_memory = 0;
        int install_builtin_modules = 1;
        if (!vm) { fprintf(stderr, "out of memory\n"); return 1; }
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--trace") == 0) scml_vm_set_trace(vm, 1);
            else if (strcmp(argv[i], "--dump-memory") == 0) dump_memory = 1;
            else if (strcmp(argv[i], "--no-builtin-modules") == 0) install_builtin_modules = 0;
            else if (strcmp(argv[i], "--trigger") == 0 && i + 1 < argc) {
                if (!scml_vm_trigger_event(vm, argv[++i], err, sizeof(err))) {
                    fprintf(stderr, "runtime error: %s\n", err);
                    scml_vm_destroy(vm);
                    return 1;
                }
            } else {
                usage(argv[0]);
                scml_vm_destroy(vm);
                return 1;
            }
        }
        if (install_builtin_modules && !scml_runtime_install_builtin_module_registry(vm)) {
            fprintf(stderr, "runtime error: cannot install builtin module registry\n");
            scml_vm_destroy(vm);
            return 1;
        }
        if (!scml_vm_load_file(vm, argv[2], err, sizeof(err)) || !scml_vm_run(vm, err, sizeof(err))) {
            fprintf(stderr, "runtime error: %s\n", err);
            scml_vm_destroy(vm);
            return 1;
        }
        if (dump_memory) scml_vm_dump_memory(vm, stdout);
        scml_vm_destroy(vm);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
