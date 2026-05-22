#include "gtasa_host.hpp"

#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s script.scmlbin\n", argv[0]);
        return 1;
    }

    SCML_VM vm;
    if (!vm.init()) {
        std::fprintf(stderr, "failed to initialize SCML VM\n");
        return 1;
    }

    openscm::gtasa::Host host;
    if (!host.registerAll(vm)) {
        std::fprintf(stderr, "failed to register GTA host natives\n");
        return 1;
    }

    if (!vm.loadScript(argv[1])) {
        std::fprintf(stderr, "load error: %s\n", vm.lastError());
        return 1;
    }

    if (!vm.run()) {
        std::fprintf(stderr, "runtime error: %s\n", vm.lastError());
        return 1;
    }

    return 0;
}
