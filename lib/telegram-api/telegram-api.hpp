#ifndef TELEGRAM_API_HPP
#define TELEGRAM_API_HPP

#include <string>
#include <filesystem>

#include "tgbot/tgbot.h"

namespace fuse_telegram_external_storage {

class TelegramApiFacade {
public:
    explicit TelegramApiFacade(const std::string& api_token);

    void sendFile(const std::string& message, std::filesystem::path& path) const;

    void longPollThread() const;

private:
    std::string api_token_;

    TgBot::Bot bot_;
};

} // fuse_telegram_external_storage

#endif //TELEGRAM_API_HPP
