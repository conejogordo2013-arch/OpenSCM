#ifndef OPENSCM_GTASA_HOST_HPP
#define OPENSCM_GTASA_HOST_HPP

#include "../../cpp/scml_vm.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace openscm::gtasa {

struct RuntimeState {
    int next_handle = 1000;
    std::unordered_map<int, int> char_health;
    std::unordered_map<int, int> car_health;
    bool radar_enabled = true;
};

class Host {
public:
    explicit Host(RuntimeState *state = nullptr) : external_state_(state) {}

    bool registerAll(SCML_VM &vm) {
        RuntimeState *state = external_state_ ? external_state_ : &owned_state_;
        bool ok = true;
        ok = reg(vm, "GTA_Log", &Host::log, state) && ok;
        ok = reg(vm, "GTA_Not", &Host::notCondition, state) && ok;
        ok = reg(vm, "GTA_PrintNow", &Host::printNow, state) && ok;
        ok = reg(vm, "GTA_PrintBig", &Host::printBig, state) && ok;
        ok = reg(vm, "GTA_ClearHelp", &Host::clearHelp, state) && ok;
        ok = reg(vm, "GTA_DisplayRadar", &Host::displayRadar, state) && ok;
        ok = reg(vm, "GTA_AddBlipForCoord", &Host::addBlipForCoord, state) && ok;
        ok = reg(vm, "GTA_RemoveBlip", &Host::removeBlip, state) && ok;
        ok = reg(vm, "GTA_RequestModel", &Host::requestModel, state) && ok;
        ok = reg(vm, "GTA_HasModelLoaded", &Host::trueCondition, state) && ok;
        ok = reg(vm, "GTA_MarkModelAsNoLongerNeeded", &Host::markModel, state) && ok;
        ok = reg(vm, "GTA_LoadScene", &Host::loadScene, state) && ok;
        ok = reg(vm, "GTA_IsPlayerPlaying", &Host::trueCondition, state) && ok;
        ok = reg(vm, "GTA_SetPlayerControl", &Host::setPlayerControl, state) && ok;
        ok = reg(vm, "GTA_GetPlayerChar", &Host::getPlayerChar, state) && ok;
        ok = reg(vm, "GTA_AddScore", &Host::addScore, state) && ok;
        ok = reg(vm, "GTA_CreateChar", &Host::createChar, state) && ok;
        ok = reg(vm, "GTA_DeleteChar", &Host::deleteChar, state) && ok;
        ok = reg(vm, "GTA_MarkCharAsNoLongerNeeded", &Host::deleteChar, state) && ok;
        ok = reg(vm, "GTA_IsCharDead", &Host::isCharDead, state) && ok;
        ok = reg(vm, "GTA_SetCharHealth", &Host::setCharHealth, state) && ok;
        ok = reg(vm, "GTA_GetCharHealth", &Host::getCharHealth, state) && ok;
        ok = reg(vm, "GTA_SetCharCoordinates", &Host::setCharCoordinates, state) && ok;
        ok = reg(vm, "GTA_TaskStandStill", &Host::taskStandStill, state) && ok;
        ok = reg(vm, "GTA_CreateCar", &Host::createCar, state) && ok;
        ok = reg(vm, "GTA_DeleteCar", &Host::deleteCar, state) && ok;
        ok = reg(vm, "GTA_MarkCarAsNoLongerNeeded", &Host::deleteCar, state) && ok;
        ok = reg(vm, "GTA_IsCarDead", &Host::isCarDead, state) && ok;
        ok = reg(vm, "GTA_SetCarHealth", &Host::setCarHealth, state) && ok;
        ok = reg(vm, "GTA_RepairCar", &Host::repairCar, state) && ok;
        ok = reg(vm, "GTA_SetCarCoordinates", &Host::setCarCoordinates, state) && ok;
        ok = reg(vm, "GTA_SetCarHeading", &Host::setCarHeading, state) && ok;
        ok = reg(vm, "GTA_SetCameraBehindPlayer", &Host::setCameraBehindPlayer, state) && ok;
        ok = reg(vm, "GTA_SetFixedCameraPosition", &Host::setFixedCameraPosition, state) && ok;
        ok = reg(vm, "GTA_RestoreCamera", &Host::restoreCamera, state) && ok;
        ok = reg(vm, "GTA_ReadMemory", &Host::readMemory, state) && ok;
        ok = reg(vm, "GTA_WriteMemory", &Host::writeMemory, state) && ok;
        ok = reg(vm, "GTA_KeyPressed", &Host::falseCondition, state) && ok;
        ok = reg(vm, "GTA_ControlKeyPressed", &Host::falseCondition, state) && ok;
        return ok;
    }

private:
    RuntimeState owned_state_;
    RuntimeState *external_state_ = nullptr;

    static bool reg(SCML_VM &vm, const char *name, ScmlNativeFunc fn, RuntimeState *state) {
        return vm.registerFunction(name, fn, state);
    }

    static RuntimeState *state(void *user_data) { return static_cast<RuntimeState *>(user_data); }

    static int asInt(const ScmlValue *args, size_t count, size_t index, int fallback = 0) {
        if (index >= count) return fallback;
        if (args[index].type == SCML_VAL_FLOAT) return static_cast<int>(args[index].real);
        return args[index].integer;
    }

    static const char *asText(const ScmlValue *args, size_t count, size_t index, const char *fallback = "") {
        if (index >= count || args[index].type != SCML_VAL_STRING || !args[index].string) return fallback;
        return args[index].string;
    }

    static void setInt(ScmlValue *ret, int value) { *ret = scml_value_int(value); }
    static int nextHandle(RuntimeState *s) { return s ? s->next_handle++ : 1; }

    static int log(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *) {
        std::printf("[SCML/GTA] %s\n", asText(args, count, 0));
        setInt(ret, 1);
        return 1;
    }

    static int notCondition(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *) {
        setInt(ret, asInt(args, count, 0) == 0 ? 1 : 0);
        return 1;
    }

    static int trueCondition(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int falseCondition(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 0); return 1; }

    static int printNow(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *) {
        std::printf("[GTA_PrintNow] %s (%d ms)\n", asText(args, count, 0), asInt(args, count, 1));
        setInt(ret, 1);
        return 1;
    }

    static int printBig(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *) {
        std::printf("[GTA_PrintBig] %s (%d ms style %d)\n", asText(args, count, 0), asInt(args, count, 1), asInt(args, count, 2));
        setInt(ret, 1);
        return 1;
    }

    static int clearHelp(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }

    static int displayRadar(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->radar_enabled = asInt(args, count, 0) != 0;
        setInt(ret, 1);
        return 1;
    }

    static int addBlipForCoord(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *user_data) {
        setInt(ret, nextHandle(state(user_data)));
        return 1;
    }

    static int removeBlip(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int requestModel(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int markModel(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int loadScene(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int setPlayerControl(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int addScore(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }

    static int getPlayerChar(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }

    static int createChar(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        int handle = nextHandle(s);
        if (s) s->char_health[handle] = 100;
        setInt(ret, handle);
        return 1;
    }

    static int deleteChar(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->char_health.erase(asInt(args, count, 0));
        setInt(ret, 1);
        return 1;
    }

    static int isCharDead(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        int handle = asInt(args, count, 0);
        int health = s && s->char_health.count(handle) ? s->char_health[handle] : 0;
        setInt(ret, health <= 0 ? 1 : 0);
        return 1;
    }

    static int setCharHealth(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->char_health[asInt(args, count, 0)] = asInt(args, count, 1, 100);
        setInt(ret, 1);
        return 1;
    }

    static int getCharHealth(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        int handle = asInt(args, count, 0);
        setInt(ret, s && s->char_health.count(handle) ? s->char_health[handle] : 0);
        return 1;
    }

    static int setCharCoordinates(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int taskStandStill(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }

    static int createCar(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        int handle = nextHandle(s);
        if (s) s->car_health[handle] = 1000;
        setInt(ret, handle);
        return 1;
    }

    static int deleteCar(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->car_health.erase(asInt(args, count, 0));
        setInt(ret, 1);
        return 1;
    }

    static int isCarDead(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        int handle = asInt(args, count, 0);
        int health = s && s->car_health.count(handle) ? s->car_health[handle] : 0;
        setInt(ret, health <= 0 ? 1 : 0);
        return 1;
    }

    static int setCarHealth(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->car_health[asInt(args, count, 0)] = asInt(args, count, 1, 1000);
        setInt(ret, 1);
        return 1;
    }

    static int repairCar(ScmlVM *, const ScmlValue *args, size_t count, ScmlValue *ret, void *user_data) {
        RuntimeState *s = state(user_data);
        if (s) s->car_health[asInt(args, count, 0)] = 1000;
        setInt(ret, 1);
        return 1;
    }

    static int setCarCoordinates(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int setCarHeading(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int setCameraBehindPlayer(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int setFixedCameraPosition(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int restoreCamera(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
    static int readMemory(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 0); return 1; }
    static int writeMemory(ScmlVM *, const ScmlValue *, size_t, ScmlValue *ret, void *) { setInt(ret, 1); return 1; }
};

} // namespace openscm::gtasa

#endif
