#include "mc3/arm/pthread_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <system_error>
#include <utility>
#include <vector>

namespace mc3::arm {
namespace {
constexpr std::uint32_t kEINVAL = 22;
constexpr std::uint32_t kEAGAIN = 11;
constexpr std::uint32_t kEBUSY = 16;
thread_local std::uint32_t current_thread_handle = 1;
thread_local std::unordered_map<std::uint32_t, std::uint32_t> thread_local_values;
}

PthreadBridge::~PthreadBridge() {
    std::vector<std::shared_ptr<ThreadRecord>> records;
    {
        std::lock_guard lock(records_mutex_);
        for (const auto& [handle, record] : threads_) {
            (void)handle;
            records.push_back(record);
        }
    }
    for (const auto& record : records) {
        if (record->thread.joinable()) {
            record->thread.join();
        }
    }
}

bool PthreadBridge::configure(
    loader::GuestMemory& memory,
    SvcDispatcher dispatcher
) {
    memory_ = &memory;
    dispatcher_ = std::move(dispatcher);
    return true;
}

bool PthreadBridge::handles(std::string_view symbol) const {
    static constexpr std::string_view names[] = {
        "pthread_create", "pthread_join", "pthread_detach",
        "pthread_equal", "pthread_self", "pthread_exit",
        "pthread_key_create", "pthread_key_delete",
        "pthread_getspecific", "pthread_setspecific",
        "pthread_setname_np", "pthread_getschedparam",
        "pthread_mutex_init", "pthread_mutex_destroy",
        "pthread_mutex_lock", "pthread_mutex_trylock", "pthread_mutex_unlock",
        "pthread_cond_init", "pthread_cond_destroy",
        "pthread_cond_wait", "pthread_cond_timedwait",
        "pthread_cond_signal", "pthread_cond_broadcast",
        "pthread_mutexattr_init", "pthread_mutexattr_destroy",
        "pthread_mutexattr_settype", "pthread_mutexattr_setpshared",
        "pthread_once"
    };
    for (const auto name : names) {
        if (symbol == name) return true;
    }
    return false;
}

std::shared_ptr<PthreadBridge::ThreadRecord>
PthreadBridge::find_thread(std::uint32_t handle) {
    std::lock_guard lock(records_mutex_);
    const auto it = threads_.find(handle);
    return it == threads_.end() ? nullptr : it->second;
}

std::shared_ptr<std::recursive_mutex>
PthreadBridge::get_mutex(std::uint32_t address) {
    if (address == 0) return {};
    std::lock_guard lock(records_mutex_);
    auto& value = mutexes_[address];
    if (!value) value = std::make_shared<std::recursive_mutex>();
    return value;
}

std::shared_ptr<PthreadBridge::ConditionRecord>
PthreadBridge::get_condition(std::uint32_t address) {
    if (address == 0) return {};
    std::lock_guard lock(records_mutex_);
    auto& value = conditions_[address];
    if (!value) value = std::make_shared<ConditionRecord>();
    return value;
}

void PthreadBridge::handle(std::string_view symbol, GuestCpu& guest) {
    const auto r0 = guest.register_value(0);
    const auto r1 = guest.register_value(1);
    const auto r2 = guest.register_value(2);
    const auto r3 = guest.register_value(3);

    if (!memory_) {
        guest.set_register(0, kEINVAL);
        return;
    }

    if (symbol == "pthread_create") {
        if (r0 == 0 || r2 == 0) {
            guest.set_register(0, kEINVAL);
            return;
        }

        std::uint32_t handle = 0;
        {
            std::lock_guard lock(records_mutex_);
            while (threads_.contains(next_thread_handle_)) ++next_thread_handle_;
            handle = next_thread_handle_++;
        }

        auto record = std::make_shared<ThreadRecord>(handle);
        const auto start = r2 & ~1u;
        const bool thumb = (r2 & 1u) != 0;
        const auto argument = r3;
        const auto dispatcher = dispatcher_;
        auto* memory = memory_;

        try {
            record->thread = std::thread(
                [record, start, thumb, argument, dispatcher, memory]() {
                    current_thread_handle = record->handle;
                    GuestCpu cpu;
                    if (cpu.initialize(memory)) {
                        if (dispatcher) cpu.set_svc_handler(dispatcher);
                        const std::array<std::uint32_t, 1> args{argument};
                        cpu.call(start, thumb, args);
                        record->return_value = cpu.register_value(0);
                    }
                }
            );
        } catch (const std::system_error&) {
            guest.set_register(0, kEAGAIN);
            return;
        }

        {
            std::lock_guard lock(records_mutex_);
            threads_.emplace(handle, record);
        }

        if (!memory_->write(r0, &handle, sizeof(handle))) {
            if (record->thread.joinable()) record->thread.join();
            std::lock_guard lock(records_mutex_);
            threads_.erase(handle);
            guest.set_register(0, kEINVAL);
            return;
        }

        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_join") {
        const auto record = find_thread(r0);
        if (!record) {
            guest.set_register(0, kEINVAL);
            return;
        }

        if (record->thread.joinable()) record->thread.join();

        if (r1 != 0 &&
            !memory_->write(r1, &record->return_value, sizeof(record->return_value))) {
            guest.set_register(0, kEINVAL);
            return;
        }

        std::lock_guard lock(records_mutex_);
        threads_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_detach") {
        const auto record = find_thread(r0);
        if (!record) {
            guest.set_register(0, kEINVAL);
            return;
        }
        if (record->thread.joinable()) record->thread.detach();
        std::lock_guard lock(records_mutex_);
        threads_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_equal") {
        guest.set_register(0, r0 == r1 ? 1u : 0u);
        return;
    }

    if (symbol == "pthread_self") {
        guest.set_register(0, current_thread_handle);
        return;
    }

    if (symbol == "pthread_exit") {
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_key_create") {
        static std::uint32_t next_key = 1;
        if (r0 == 0) {
            guest.set_register(0, kEINVAL);
            return;
        }
        const auto key = next_key++;
        if (!memory_->write(r0, &key, sizeof(key))) {
            guest.set_register(0, kEINVAL);
            return;
        }
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_key_delete") {
        thread_local_values.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_getspecific") {
        const auto it = thread_local_values.find(r0);
        guest.set_register(
            0,
            it == thread_local_values.end() ? 0u : it->second
        );
        return;
    }

    if (symbol == "pthread_setspecific") {
        thread_local_values[r0] = r1;
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_setname_np" ||
        symbol == "pthread_getschedparam") {
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_mutexattr_init" ||
        symbol == "pthread_mutexattr_destroy" ||
        symbol == "pthread_mutexattr_settype" ||
        symbol == "pthread_mutexattr_setpshared") {
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_mutex_init") {
        guest.set_register(0, get_mutex(r0) ? 0u : kEINVAL);
        return;
    }

    if (symbol == "pthread_mutex_destroy") {
        std::lock_guard lock(records_mutex_);
        mutexes_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_mutex_lock") {
        const auto mutex = get_mutex(r0);
        if (!mutex) {
            guest.set_register(0, kEINVAL);
            return;
        }
        mutex->lock();
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_mutex_trylock") {
        const auto mutex = get_mutex(r0);
        if (!mutex) {
            guest.set_register(0, kEINVAL);
            return;
        }
        guest.set_register(0, mutex->try_lock() ? 0u : kEBUSY);
        return;
    }

    if (symbol == "pthread_mutex_unlock") {
        const auto mutex = get_mutex(r0);
        if (!mutex) {
            guest.set_register(0, kEINVAL);
            return;
        }
        try {
            mutex->unlock();
            guest.set_register(0, 0);
        } catch (const std::system_error&) {
            guest.set_register(0, kEINVAL);
        }
        return;
    }

    if (symbol == "pthread_cond_init") {
        guest.set_register(0, get_condition(r0) ? 0u : kEINVAL);
        return;
    }

    if (symbol == "pthread_cond_destroy") {
        std::lock_guard lock(records_mutex_);
        conditions_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_cond_signal" ||
        symbol == "pthread_cond_broadcast") {
        const auto condition = get_condition(r0);
        if (!condition) {
            guest.set_register(0, kEINVAL);
            return;
        }
        if (symbol == "pthread_cond_signal") condition->condition.notify_one();
        else condition->condition.notify_all();
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_cond_wait" ||
        symbol == "pthread_cond_timedwait") {
        const auto condition = get_condition(r0);
        const auto mutex = get_mutex(r1);
        if (!condition || !mutex) {
            guest.set_register(0, kEINVAL);
            return;
        }

        std::unique_lock<std::recursive_mutex> lock(*mutex, std::adopt_lock);

        if (symbol == "pthread_cond_wait") {
            condition->condition.wait(lock);
        } else {
            std::chrono::system_clock::time_point deadline =
                std::chrono::system_clock::now();

            if (r2 != 0 && memory_->contains(r2, 8)) {
                std::int32_t seconds = 0;
                std::int32_t nanos = 0;
                std::memcpy(&seconds, memory_->data(r2), sizeof(seconds));
                std::memcpy(&nanos, memory_->data(r2 + 4), sizeof(nanos));
                const auto guest_duration =
                    std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanos);
                deadline = std::chrono::system_clock::time_point(
                    std::chrono::duration_cast<std::chrono::system_clock::duration>(guest_duration)
                );
            }
            condition->condition.wait_until(lock, deadline);
        }

        lock.release();
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "pthread_once") {
        if (r0 != 0 && memory_->contains(r0, 4)) {
            const std::int32_t initialized = 1;
            memory_->write(r0, &initialized, sizeof(initialized));
        }
        (void)r1;
        guest.set_register(0, 0);
        return;
    }

    guest.set_register(0, 0);
}

} // namespace mc3::arm
