#ifndef SCML_CPP_VM_HPP
#define SCML_CPP_VM_HPP

#include "../vm/vm.h"

#include <stdexcept>
#include <string>

class SCML_VM {
public:
    SCML_VM() = default;
    ~SCML_VM() { shutdown(); }

    SCML_VM(const SCML_VM &) = delete;
    SCML_VM &operator=(const SCML_VM &) = delete;

    bool init() {
        shutdown();
        vm_ = scml_vm_create();
        return vm_ != nullptr;
    }

    void shutdown() {
        if (vm_) scml_vm_destroy(vm_);
        vm_ = nullptr;
    }

    bool loadScript(const char *path) {
        ensure();
        return scml_vm_load_file(vm_, path, lastError_, sizeof(lastError_)) != 0;
    }

    bool registerFunction(const char *name, ScmlNativeFunc fn, void *userData = nullptr) {
        ensure();
        return scml_vm_register_function(vm_, name, fn, userData) != 0;
    }

    bool triggerEvent(const char *name) {
        ensure();
        return scml_vm_trigger_event(vm_, name, lastError_, sizeof(lastError_)) != 0;
    }

    bool run() {
        ensure();
        return scml_vm_run(vm_, lastError_, sizeof(lastError_)) != 0;
    }

    bool update() {
        ensure();
        return scml_vm_update(vm_, lastError_, sizeof(lastError_)) != 0;
    }

    void setTrace(bool enabled) {
        ensure();
        scml_vm_set_trace(vm_, enabled ? 1 : 0);
    }

    bool setInt(const char *name, int value) { ensure(); return scml_vm_set_int(vm_, name, value) != 0; }
    bool setFloat(const char *name, float value) { ensure(); return scml_vm_set_float(vm_, name, value) != 0; }
    bool setString(const char *name, const char *value) { ensure(); return scml_vm_set_string(vm_, name, value) != 0; }

    const char *lastError() const { return lastError_; }
    ScmlVM *raw() { return vm_; }
    const ScmlVM *raw() const { return vm_; }

private:
    void ensure() const {
        if (!vm_) throw std::runtime_error("SCML_VM is not initialized");
    }

    ScmlVM *vm_ = nullptr;
    char lastError_[1024] = {0};
};

#endif
