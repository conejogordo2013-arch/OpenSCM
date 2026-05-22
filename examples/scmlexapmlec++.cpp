#include "../cpp/scml_vm.hpp"
#include <iostream>

int main() {
    SCML_VM vm;
    if (!vm.init()) return 1;
    vm.setTrace(true);
    if (!vm.loadScript("c-c++-c#-compatibilityexample.scmlbin") || !vm.run()) {
        std::cerr << vm.lastError() << '\n';
        return 1;
    }
    return 0;
}
