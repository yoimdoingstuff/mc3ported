#pragma once
#include <filesystem>
#include <optional>
namespace mc3 {
struct Config {
    std::filesystem::path game_data;
    std::filesystem::path obb_root;
    std::filesystem::path runtime_root;
    std::filesystem::path save_root;
};
std::filesystem::path default_runtime_root();
std::filesystem::path default_save_root();
std::optional<Config> load_config(int argc, char** argv);
} // namespace mc3
