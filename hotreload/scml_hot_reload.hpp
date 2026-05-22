#ifndef SCML_HOT_RELOAD_HPP
#define SCML_HOT_RELOAD_HPP

#include "../cpp/scml_vm.hpp"

#include <filesystem>
#include <string>

class SCML_HotReload {
public:
    bool watch(SCML_VM *vm, std::string bytecodePath) {
        vm_ = vm;
        path_ = std::move(bytecodePath);
        if (!std::filesystem::exists(path_)) return false;
        lastWrite_ = std::filesystem::last_write_time(path_);
        return true;
    }

    bool poll() {
        if (!vm_ || path_.empty() || !std::filesystem::exists(path_)) return false;
        auto now = std::filesystem::last_write_time(path_);
        if (now == lastWrite_) return false;
        lastWrite_ = now;
        scml_vm_clear_events(vm_->raw());
        return vm_->loadScript(path_.c_str()) && vm_->run();
    }

private:
    SCML_VM *vm_ = nullptr;
    std::string path_;
    std::filesystem::file_time_type lastWrite_{};
};

#endif
