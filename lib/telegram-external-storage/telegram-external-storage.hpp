#ifndef TELEGRAM_EXTERNAL_STORAGE_HPP
#define TELEGRAM_EXTERNAL_STORAGE_HPP

#include <string>
#include <thread>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "lib/telegram-api/telegram-api.hpp"
#include "lib/external-storage-interface.hpp"

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

namespace fuse_telegram_external_storage {

    class TelegramExternalStorage final : public fuse_external_storage::ExternalStorageInterface {
    public:
        explicit TelegramExternalStorage(const std::string& api_token);
        ~TelegramExternalStorage() override;

        struct stat getAttr(std::filesystem::path& path) override;
        std::vector<FileInfo> listDir(const std::filesystem::path& path) override;
        int createFile(const std::filesystem::path& path, mode_t mode) override;
        int readFile(const std::filesystem::path& path, char* buf, size_t size, off_t offset) override;
        int writeFile(const std::filesystem::path& path, const char* buf, size_t size, off_t offset) override;
        int unlinkFile(const std::filesystem::path& path) override;
        int createDir(const std::filesystem::path& path, mode_t mode) override;
        int removeDir(const std::filesystem::path& path) override;
        int rename(const std::filesystem::path& from, const std::filesystem::path& to) override;

    private:
        TelegramApiFacade api_;
        std::thread bot_thread_;

        // Helper methods
        nlohmann::json getMetadata() const;
        void updateMetadata(const nlohmann::json& metadata) const;
        FileInfo* findFileInfo(const std::filesystem::path& path, nlohmann::json& metadata) const;
        void addFileInfo(const std::filesystem::path& path, int64_t message_id, size_t size, bool is_dir, nlohmann::json& metadata) const;
        void removeFileInfo(const std::filesystem::path& path, nlohmann::json& metadata) const;
    };

} // namespace fuse_telegram_external_storage

#endif // TELEGRAM_EXTERNAL_STORAGE_HPP