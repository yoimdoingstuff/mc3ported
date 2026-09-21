#include "mc3/apk_archive.hpp"

#include "mc3/log.hpp"

#include <miniz_zip.h>

#include <filesystem>
#include <string>

namespace mc3 {
namespace {

class ZipReader {
public:
    ZipReader() {
        mz_zip_zero_struct(&zip_);
    }

    ~ZipReader() {
        if (initialized_) {
            mz_zip_reader_end(&zip_);
        }
    }

    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    bool open(const std::filesystem::path& path) {
        if (initialized_) {
            mz_zip_reader_end(&zip_);
            initialized_ = false;
        }

        const auto native = path.u8string();
        if (!mz_zip_reader_init_file(
                &zip_,
                reinterpret_cast<const char*>(native.c_str()),
                0
            )) {
            log_error("Failed to open APK as ZIP: " + path.string());
            return false;
        }

        initialized_ = true;
        return true;
    }

    bool contains(const std::string& name) {
        return mz_zip_reader_locate_file(
            &zip_,
            name.c_str(),
            nullptr,
            MZ_ZIP_FLAG_CASE_SENSITIVE
        ) >= 0;
    }

    bool extract(
        const std::string& name,
        const std::filesystem::path& destination
    ) {
        const auto index = mz_zip_reader_locate_file(
            &zip_,
            name.c_str(),
            nullptr,
            MZ_ZIP_FLAG_CASE_SENSITIVE
        );

        if (index < 0) {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec) {
            log_error(
                "Failed to create APK extraction directory: " +
                destination.parent_path().string() +
                " (" + ec.message() + ")"
            );
            return false;
        }

        const auto native = destination.u8string();
        if (!mz_zip_reader_extract_to_file(
                &zip_,
                static_cast<mz_uint>(index),
                reinterpret_cast<const char*>(native.c_str()),
                0
            )) {
            log_error(
                "Failed to extract APK entry '" + name +
                "' to '" + destination.string() + "'"
            );
            return false;
        }

        return true;
    }

private:
    mz_zip_archive zip_{};
    bool initialized_ = false;
};

} // namespace

bool extract_apk_entry(
    const std::filesystem::path& apk_path,
    std::string_view entry_name,
    const std::filesystem::path& destination
) {
    if (apk_path.empty() || entry_name.empty() || destination.empty()) {
        return false;
    }

    ZipReader archive;
    if (!archive.open(apk_path)) {
        return false;
    }

    return archive.extract(std::string(entry_name), destination);
}

bool apk_contains_entry(
    const std::filesystem::path& apk_path,
    std::string_view entry_name
) {
    if (apk_path.empty() || entry_name.empty()) {
        return false;
    }

    ZipReader archive;
    if (!archive.open(apk_path)) {
        return false;
    }

    return archive.contains(std::string(entry_name));
}

} // namespace mc3
