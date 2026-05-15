#include "../compiler/compiler.h"
#include "../cpp/scml_vm.hpp"
#include "../debugger/debugger.h"
#include "../graphics/scml_graphics.hpp"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const char *source = argc > 1 ? argv[1] : "examples/gameplay.scml";
    const char *bytecode = argc > 2 ? argv[2] : "examples/editor_tmp.scmlbin";
    char err[1024] = {0};

    std::cout << "SCML Visual Editor prototype\n";
    std::cout << "Panels: file explorer | SCM text editor | compile/run/debug buttons | output console\n";

    if (!scml_compile_file(source, bytecode, err, sizeof(err))) {
        std::cerr << "compile failed: " << err << '\n';
        return 1;
    }

    SCML_VM vm;
    if (!vm.init() || !vm.loadScript(bytecode)) {
        std::cerr << vm.lastError() << '\n';
        return 1;
    }

    ScmlDebugger debugger;
    scml_debugger_init(&debugger, vm.raw());
    scml_debugger_add_label_breakpoint(&debugger, "MAIN");
    scml_debugger_dump_state(&debugger, stdout);

    SCML_Graphics graphics;
    graphics.init("SCML Visual Editor", 960, 540);
    graphics.shutdown();
    return 0;
}
