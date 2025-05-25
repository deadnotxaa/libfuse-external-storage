#ifndef TELEGRAM_EXTERNAL_STORAGE_HPP
#define TELEGRAM_EXTERNAL_STORAGE_HPP

#include <string>
#include <thread>
#include <filesystem>

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>

#include "lib/telegram-api/telegram-api.hpp"
#include "lib/external-storage-interface.hpp"

namespace fuse_telegram_external_storage {

class TelegramExternalStorage final : public fuse_external_storage::ExternalStorageInterface {
public:
    explicit TelegramExternalStorage(const std::string& api_token);

    struct stat getAttr(std::filesystem::path&) override;

    ~TelegramExternalStorage() override;

private:
    TelegramApiFacade api_;
    std::thread bot_thread_;
};

} // fuse_telegram_external_storage

#endif //TELEGRAM_EXTERNAL_STORAGE_HPP
