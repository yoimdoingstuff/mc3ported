#include "mc3/log.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
namespace mc3 {
namespace {
std::mutex g_log_mutex;
const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warn";
        case LogLevel::Error: return "error";
    }
    return "unknown";
}
}
void log(LogLevel level, std::string_view message) {
    std::lock_guard lock(g_log_mutex);
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::fprintf(stderr, "[%02d:%02d:%02d] [%s] %.*s\n",
        tm.tm_hour, tm.tm_min, tm.tm_sec, level_name(level),
        static_cast<int>(message.size()), message.data());
}
void log_debug(std::string_view message) { log(LogLevel::Debug, message); }
void log_info(std::string_view message) { log(LogLevel::Info, message); }
void log_warning(std::string_view message) { log(LogLevel::Warning, message); }
void log_error(std::string_view message) { log(LogLevel::Error, message); }
} // namespace mc3
