#pragma once

#include "mc3/loader/memory.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace mc3::arm {

class GuestCpu;

class PthreadBridge {
public:
    using SvcDispatcher =
        std::function<void(std::uint32_t, GuestCpu&)>;

    ~PthreadBridge();

    bool configure(
        loader::GuestMemory& memory,
        SvcDispatcher dispatcher
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

private:
    struct ThreadRecord {
        explicit ThreadRecord(std::uint32_t id)
            : handle(id) {}

        std::uint32_t handle = 0;
        std::thread thread;
        std::mutex state_mutex;
        std::uint32_t return_value = 0;
    };

    std::shared_ptr<ThreadRecord> find_thread(std::uint32_t handle);
    std::shared_ptr<std::recursive_mutex> get_mutex(std::uint32_t address);

    struct ConditionRecord {
        std::condition_variable_any condition;
    };

    std::shared_ptr<ConditionRecord> get_condition(std::uint32_t address);

    loader::GuestMemory* memory_ = nullptr;
    SvcDispatcher dispatcher_{};

    std::mutex records_mutex_;
    std::unordered_map<std::uint32_t, std::shared_ptr<ThreadRecord>> threads_;
    std::unordered_map<std::uint32_t, std::shared_ptr<std::recursive_mutex>> mutexes_;
    std::unordered_map<std::uint32_t, std::shared_ptr<ConditionRecord>> conditions_;

    std::uint32_t next_thread_handle_ = 2;
};

} // namespace mc3::arm
