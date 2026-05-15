#include "../vm/vm.h"
#include <stdio.h>

int main(void) {
    char err[256] = {0};
    ScmlVM *vm = scml_vm_create();
    if (!vm) return 1;
    if (!scml_vm_load_file(vm, "c-c++-c#-compatibilityexample.scmlbin", err, sizeof(err)) ||
        !scml_vm_run(vm, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        scml_vm_destroy(vm);
        return 1;
    }
    scml_vm_destroy(vm);
    return 0;
}
