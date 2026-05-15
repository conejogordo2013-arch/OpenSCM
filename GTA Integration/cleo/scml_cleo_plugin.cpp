// OpenSCM GTA SA CLEO/ASI integration skeleton.
//
// This file is intentionally not part of the default OpenSCM build because real
// CLEO/ASI builds need the target SDK, injector entry points, and GTA SA symbol
// addresses supplied by the plugin project.  Keep this layer outside the SCML
// language: it loads .scmlbin files, registers GTA natives, and ticks the VM.

#include "../host/gtasa_host.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace openscm::gtasa::cleo {

struct LoadedScript {
    std::string path;
    std::unique_ptr<SCML_VM> vm;
    Host host;
};

class PluginRuntime {
public:
    bool loadScript(const char *path) {
        LoadedScript script;
        script.path = path;
        script.vm = std::make_unique<SCML_VM>();
        if (!script.vm->init()) return false;
        if (!script.host.registerAll(*script.vm)) return false;
        if (!script.vm->loadScript(path)) {
            std::fprintf(stderr, "[OpenSCM CLEO] load error in %s: %s\n", path, script.vm->lastError());
            return false;
        }
        scripts_.push_back(std::move(script));
        return true;
    }

    void tick() {
        for (LoadedScript &script : scripts_) {
            if (!script.vm->update()) {
                std::fprintf(stderr, "[OpenSCM CLEO] runtime error in %s: %s\n", script.path.c_str(), script.vm->lastError());
            }
        }
    }

private:
    std::vector<LoadedScript> scripts_;
};

} // namespace openscm::gtasa::cleo

/*
Suggested plugin entry wiring:

  static openscm::gtasa::cleo::PluginRuntime g_scml;

  extern "C" void OnPluginLoad() {
      g_scml.loadScript("CLEO/SCML/spawn_demo.scmlbin");
  }

  extern "C" void OnGameFrame() {
      g_scml.tick();
  }

Replace OnPluginLoad/OnGameFrame with the callbacks from the CLEO/ASI SDK you
use.  Real GTA behavior belongs in host native implementations, not in SCML.
*/
