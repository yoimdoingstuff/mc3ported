#pragma once
#include <string_view>
namespace mc3 {
enum class LogLevel { Debug, Info, Warning, Error };
void log(LogLevel level, std::string_view message);
void log_debug(std::string_view message);
void log_info(std::string_view message);
void log_warning(std::string_view message);
void log_error(std::string_view message);
} // namespace mc3
