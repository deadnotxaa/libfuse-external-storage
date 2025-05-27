#ifndef EXTERNAL_STORAGE_INTERFACE_HPP
#define EXTERNAL_STORAGE_INTERFACE_HPP

#include <filesystem>
#include <vector>
#include "lib/telegram-api/telegram-api.hpp"

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

namespace fuse_external_storage {

    class ExternalStorageInterface {
    public:
        virtual struct stat getAttr(std::filesystem::path& path) = 0;
        virtual std::vector<fuse_telegram_external_storage::FileInfo> listDir(const std::filesystem::path& path) = 0;
        virtual int createFile(const std::filesystem::path& path, mode_t mode) = 0;
        virtual int readFile(const std::filesystem::path& path, char* buf, size_t size, off_t offset) = 0;
        virtual int writeFile(const std::filesystem::path& path, const char* buf, size_t size, off_t offset) = 0;
        virtual int unlinkFile(const std::filesystem::path& path) = 0;
        virtual int createDir(const std::filesystem::path& path, mode_t mode) = 0;
        virtual int removeDir(const std::filesystem::path& path) = 0;
        virtual int rename(const std::filesystem::path& from, const std::filesystem::path& to) = 0;

        virtual ~ExternalStorageInterface() = default;
    };

} // namespace fuse_external_storage

#endif // EXTERNAL_STORAGE_INTERFACE_HPP