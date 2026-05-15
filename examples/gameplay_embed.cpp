#include "../cpp/scml_vm.hpp"

#include <iostream>
#include <string>
#include <vector>

struct Entity {
    int id;
    std::string model;
    float x;
    float y;
    float z;
    int health;
};

struct GameWorld {
    std::vector<Entity> entities;
    int nextId = 1;
};

static const char *asString(const ScmlValue &v) {
    return v.type == SCML_VAL_STRING && v.string ? v.string : "";
}

static float asFloat(const ScmlValue &v) {
    if (v.type == SCML_VAL_FLOAT) return v.real;
    if (v.type == SCML_VAL_INT) return static_cast<float>(v.integer);
    return v.string ? std::stof(v.string) : 0.0f;
}

static int SpawnEnemy(ScmlVM *, const ScmlValue *args, size_t argCount, ScmlValue *ret, void *userData) {
    if (argCount < 4) return 0;
    auto *world = static_cast<GameWorld *>(userData);
    Entity e{world->nextId++, asString(args[0]), asFloat(args[1]), asFloat(args[2]), asFloat(args[3]), 100};
    world->entities.push_back(e);
    std::cout << "SpawnEnemy id=" << e.id << " model=" << e.model << " pos=" << e.x << ',' << e.y << ',' << e.z << '\n';
    *ret = scml_value_int(e.id);
    return 1;
}

static int SpawnMedkit(ScmlVM *, const ScmlValue *args, size_t argCount, ScmlValue *ret, void *userData) {
    if (argCount < 3) return 0;
    auto *world = static_cast<GameWorld *>(userData);
    Entity e{world->nextId++, "medkit", asFloat(args[0]), asFloat(args[1]), asFloat(args[2]), 0};
    world->entities.push_back(e);
    std::cout << "SpawnMedkit id=" << e.id << " pos=" << e.x << ',' << e.y << ',' << e.z << '\n';
    *ret = scml_value_int(e.id);
    return 1;
}

static int Log(ScmlVM *, const ScmlValue *args, size_t argCount, ScmlValue *ret, void *) {
    if (argCount > 0) std::cout << "[SCML] " << asString(args[0]) << '\n';
    *ret = scml_value_int(0);
    return 1;
}

int main() {
    GameWorld world;
    SCML_VM vm;
    if (!vm.init()) return 1;
    if (!vm.loadScript("examples/gameplay.scmlbin")) {
        std::cerr << vm.lastError() << '\n';
        return 1;
    }

    vm.registerFunction("SpawnEnemy", SpawnEnemy, &world);
    vm.registerFunction("SpawnMedkit", SpawnMedkit, &world);
    vm.registerFunction("Log", Log, nullptr);

    vm.setFloat("$PLAYER_X", 1.0f);
    vm.setFloat("$PLAYER_Y", 2.0f);
    vm.setFloat("$PLAYER_Z", 3.0f);
    vm.setInt("$PLAYER_HEALTH", 45);

    vm.triggerEvent("ON_START");
    vm.run();

    for (int frame = 0; frame < 2; ++frame) {
        vm.triggerEvent("ON_TICK");
        vm.update();
    }

    vm.triggerEvent("ON_PLAYER_DAMAGE");
    vm.update();
    return 0;
}
