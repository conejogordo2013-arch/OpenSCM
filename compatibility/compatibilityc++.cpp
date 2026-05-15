#include "../cpp/scml_vm.hpp"
extern "C" int scml_compat_cpp_smoke() { SCML_VM vm; return vm.init() ? 1 : 0; }
