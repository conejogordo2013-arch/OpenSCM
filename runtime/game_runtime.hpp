#ifndef SCML_GAME_RUNTIME_HPP
#define SCML_GAME_RUNTIME_HPP

#include "../cpp/scml_vm.hpp"

#include <string>
#include <vector>

struct SCML_Entity {
    int id = 0;
    std::string model;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int health = 100;
};

class SCML_GameRuntime {
public:
    bool init(const char *scriptPath) {
        if (!vm.init() || !vm.loadScript(scriptPath)) return false;
        vm.registerFunction("SpawnEntity", &SCML_GameRuntime::spawnEntityNative, this);
        vm.registerFunction("SetEntityHealth", &SCML_GameRuntime::setEntityHealthNative, this);
        vm.registerFunction("Log", &SCML_GameRuntime::logNative, this);
        return true;
    }

    bool start() { return vm.triggerEvent("ON_START") && vm.run(); }
    bool tick() { return vm.triggerEvent("ON_TICK") && vm.update(); }

    SCML_VM vm;
    std::vector<SCML_Entity> entities;
    int nextEntityId = 1;

private:
    static const char *asString(const ScmlValue &v) { return v.type == SCML_VAL_STRING && v.string ? v.string : ""; }
    static float asFloat(const ScmlValue &v) { return v.type == SCML_VAL_FLOAT ? v.real : (v.type == SCML_VAL_INT ? (float)v.integer : 0.0f); }

    static int spawnEntityNative(ScmlVM *, const ScmlValue *args, size_t argc, ScmlValue *ret, void *user) {
        if (argc < 4) return 0;
        auto *runtime = static_cast<SCML_GameRuntime *>(user);
        SCML_Entity entity;
        entity.id = runtime->nextEntityId++;
        entity.model = asString(args[0]);
        entity.x = asFloat(args[1]);
        entity.y = asFloat(args[2]);
        entity.z = asFloat(args[3]);
        runtime->entities.push_back(entity);
        *ret = scml_value_int(entity.id);
        return 1;
    }

    static int setEntityHealthNative(ScmlVM *, const ScmlValue *args, size_t argc, ScmlValue *ret, void *user) {
        if (argc < 2) return 0;
        auto *runtime = static_cast<SCML_GameRuntime *>(user);
        int id = args[0].type == SCML_VAL_INT ? args[0].integer : 0;
        int health = args[1].type == SCML_VAL_INT ? args[1].integer : 0;
        for (auto &entity : runtime->entities) if (entity.id == id) entity.health = health;
        *ret = scml_value_int(0);
        return 1;
    }

    static int logNative(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) {
        *ret = scml_value_int(0);
        return 1;
    }
};

#endif
