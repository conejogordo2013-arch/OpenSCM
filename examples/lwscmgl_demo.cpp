#include "../cpp/scml_vm.hpp"
#include "../runtime/lwscmgl.hpp"
#include <iostream>

int main() {
    SCML_VM vm;
    if (!vm.init()) return 1;

    LWSCMGL_Context ctx;
    lwscmgl_register_basics(vm, ctx);

    if (!vm.loadScript("examples/lwscmgl_demo.scmlbin")) return 1;
    if (!vm.run()) return 1;

    std::cout << "LWSCMGL commands captured: " << ctx.commands().size() << "\n";
    for (const auto &c : ctx.commands()) {
        std::cout << c.kind << " pos(" << c.x << "," << c.y << "," << c.z << ") color(" << c.r << "," << c.g << "," << c.b << "," << c.a << ")\n";
    }
    return 0;
}
