#ifndef LWSCMGL_HPP
#define LWSCMGL_HPP

#include "../cpp/scml_vm.hpp"
#include <string>
#include <vector>

struct LWSCMGL_DrawCmd {
    std::string kind;
    float x, y, z;
    float r, g, b, a;
};

class LWSCMGL_Context {
public:
    void clear() { cmds.clear(); }
    void drawPoint(float x, float y, float z, float r, float g, float b, float a) {
        cmds.push_back({"point", x, y, z, r, g, b, a});
    }
    const std::vector<LWSCMGL_DrawCmd>& commands() const { return cmds; }
private:
    std::vector<LWSCMGL_DrawCmd> cmds;
};

inline int LWSCMGL_DrawPointNative(ScmlVM *vm, const ScmlValue *args, size_t argc, ScmlValue *ret, void *user) {
    (void)vm; (void)ret;
    if (!user || argc < 7) return 0;
    LWSCMGL_Context *ctx = static_cast<LWSCMGL_Context*>(user);
    ctx->drawPoint((float)args[0].real, (float)args[1].real, (float)args[2].real,
                   (float)args[3].real, (float)args[4].real, (float)args[5].real,
                   (float)args[6].real);
    return 1;
}

inline void lwscmgl_register_basics(SCML_VM &vm, LWSCMGL_Context &ctx) {
    vm.registerFunction("LWSCMGL_DrawPoint", LWSCMGL_DrawPointNative, &ctx);
}

#endif
