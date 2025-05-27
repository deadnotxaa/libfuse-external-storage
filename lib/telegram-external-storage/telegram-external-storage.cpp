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
    struct stat st = {};



    return st;
}

json ftes::TelegramExternalStorage::getMetadata() const {
    json metadata = api_.getMetadata();

    if (metadata.is_null()) {
        metadata = json{{"files", json::array()}};

        std::cerr << "[ftes::TelegramExternalStorage::getMetadata()]: " << metadata.dump() << std::endl;
    }

    return metadata;
}
