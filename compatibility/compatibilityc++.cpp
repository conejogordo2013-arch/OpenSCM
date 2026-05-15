#include "../vm/vm.h"
extern "C" int scml_compat_cpp_smoke() { ScmlVM *vm = scml_vm_create(); scml_vm_destroy(vm); return vm != nullptr; }
