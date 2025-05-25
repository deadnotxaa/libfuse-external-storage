#include "telegram-external-storage.hpp"

#include <thread>

namespace ftes = fuse_telegram_external_storage;

ftes::TelegramExternalStorage::TelegramExternalStorage(const std::string& api_token)
    : api_(TelegramApiFacade(api_token))
{
    bot_thread_ = std::thread([this] {
        api_.longPollThread();
    });
}

ftes::TelegramExternalStorage::~TelegramExternalStorage()
{
    if (bot_thread_.joinable()) {
        bot_thread_.join();
    }
}

struct stat ftes::TelegramExternalStorage::getAttr(std::filesystem::path& path) {
    return {};
}
