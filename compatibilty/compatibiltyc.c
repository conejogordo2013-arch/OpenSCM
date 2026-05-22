#include "../vm/vm.h"
int scml_compat_c_smoke(void) { ScmlVM *vm = scml_vm_create(); scml_vm_trigger_event(vm, "HOST_EVENT", 0, 0); scml_vm_destroy(vm); return vm != 0; }
