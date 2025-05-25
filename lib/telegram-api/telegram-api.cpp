#include "telegram-api.hpp"

namespace ftes = fuse_telegram_external_storage;

ftes::TelegramApiFacade::TelegramApiFacade(const std::string& api_token)
    : api_token_(api_token), bot_(api_token_)
{
    bot_.getEvents().onCommand("id", [this](TgBot::Message::Ptr message) {
        bot_.getApi().sendMessage(message->chat->id, std::to_string(message->chat->id));
    });
}

void ftes::TelegramApiFacade::longPollThread() const {
    // TODO: Chat ID -4940199065

    try {
        // printf("Bot username: %s\n", bot_.getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(bot_);
        while (true) {
            // printf("Long poll started\n");
            longPoll.start();
        }
    } catch (TgBot::TgException& e) {
        printf("error: %s\n", e.what());
    }
}
