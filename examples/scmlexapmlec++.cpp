#include "../vm/vm.h"
#include <iostream>

int main() {
    char err[256] = {};
    ScmlVM *vm = scml_vm_create();
    if (!vm) return 1;
    scml_vm_set_trace(vm, 1);
    if (!scml_vm_load_file(vm, "c-c++-c#-compatibilityexample.scmlbin", err, sizeof(err)) ||
        !scml_vm_run(vm, err, sizeof(err))) {
        std::cerr << err << '\n';
        scml_vm_destroy(vm);
        return 1;
    }
    scml_vm_destroy(vm);
    return 0;
}
