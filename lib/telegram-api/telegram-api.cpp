#include "telegram-api.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ftes = fuse_telegram_external_storage;

using json = nlohmann::json;

ftes::TelegramApiFacade::TelegramApiFacade(const std::string& api_token)
    : api_token_(api_token), bot_(api_token_) {
    // Set up the /start command to save chat ID
    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr message) {
        int64_t chat_id = message->chat->id;

        // Добавьте это для отладки
        std::cerr << "Attempting to open chat_id.txt in CWD: " << std::filesystem::current_path() << std::endl
        << chat_id_file_ << std::endl;

        std::ofstream ofs(chat_id_file_, std::ios::out);

        if (ofs.is_open()) {
            ofs << chat_id;
            ofs.close();
            [[maybe_unused]] auto result = bot_.getApi().sendMessage(chat_id, "Chat ID saved. Bot is ready!");
        } else {
            [[maybe_unused]] auto result = bot_.getApi().sendMessage(chat_id, "Error saving chat ID.");
        }
    });

    // Initialize metadata message ID
    try {
        auto metadata = getMetadata();
        if (!metadata.is_null()) {
            std::ifstream ifs(std::getenv("HOME") + std::string("/metadata_message_id.txt"));
            if (ifs) {
                ifs >> metadata_message_id_;
                ifs.close();
            }
        }
    } catch (const std::exception& e) {
        // No metadata message yet; will be created on first update
    }
}

void ftes::TelegramApiFacade::longPollThread() {
    try {
        TgBot::TgLongPoll longPoll(bot_);
        while (true) {
            longPoll.start();
        }
    } catch (const TgBot::TgException& e) {
        printf("Telegram API error: %s\n", e.what());
    }
}

int64_t ftes::TelegramApiFacade::sendFile(const std::filesystem::path& path, const std::string& file_name) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        throw std::runtime_error("Chat ID not set");
    }

    TgBot::InputFile::Ptr input_file = TgBot::InputFile::fromFile(path.string(), "application/octet-stream");
    auto message = bot_.getApi().sendDocument(chat_id, input_file, file_name);
    return message->messageId;
}

bool ftes::TelegramApiFacade::downloadFile(int64_t message_id, const std::filesystem::path& dest_path) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return false;
    }

    try {
        // Fetch recent updates to find the message with the specified message_id
        std::string file_id;
        auto updates = bot_.getApi().getUpdates(0, 100); // Limit to 100 recent updates
        for (const auto& update : updates) {
            if (update->message && update->message->messageId == message_id && update->message->document) {
                file_id = update->message->document->fileId;
                break;
            }
        }
        if (file_id.empty()) {
            printf("File not found for message ID: %lld\n", message_id);
            return false;
        }

        // Get file path from Telegram
        auto file = bot_.getApi().getFile(file_id);
        // Download file content as string
        std::string file_content = bot_.getApi().downloadFile(file->filePath);
        // Write content to destination file
        std::ofstream ofs(dest_path, std::ios::binary);
        if (!ofs) {
            printf("Failed to open destination file: %s\n", dest_path.c_str());
            return false;
        }
        ofs.write(file_content.data(), file_content.size());
        ofs.close();
        return true;
    } catch (const TgBot::TgException& e) {
        printf("Error downloading file: %s\n", e.what());
        return false;
    }
}

bool ftes::TelegramApiFacade::deleteMessage(int64_t message_id) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return false;
    }

    try {
        return bot_.getApi().deleteMessage(chat_id, message_id);
    } catch (const TgBot::TgException& e) {
        printf("Error deleting message: %s\n", e.what());
        return false;
    }
}

int64_t ftes::TelegramApiFacade::updateMetadata(const json& metadata) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        throw std::runtime_error("Chat ID not set");
    }

    std::string metadata_str = metadata.dump();
    int64_t new_message_id;
    try {
        auto message = bot_.getApi().sendMessage(chat_id, metadata_str);
        new_message_id = message->messageId;
        // Pin the new metadata message
        bot_.getApi().pinChatMessage(chat_id, new_message_id, true);
        // Save new metadata message ID
        std::ofstream ofs(std::getenv("HOME") + std::string("/metadata_message_id.txt"));
        if (ofs) {
            ofs << new_message_id;
            ofs.close();
        }
        // Unpin and delete old metadata message if it exists
        if (metadata_message_id_ != 0) {
            bot_.getApi().unpinChatMessage(chat_id, metadata_message_id_);
            bot_.getApi().deleteMessage(chat_id, metadata_message_id_);
        }
        metadata_message_id_ = new_message_id;
        return new_message_id;
    } catch (const TgBot::TgException& e) {
        printf("Error updating metadata: %s\n", e.what());
        throw;
    }
}

json ftes::TelegramApiFacade::getMetadata() const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return json{};
    }

    try {
        auto chat = bot_.getApi().getChat(chat_id);
        if (chat->pinnedMessage && !chat->pinnedMessage->text.empty()) {
            metadata_message_id_ = chat->pinnedMessage->messageId;
            return json::parse(chat->pinnedMessage->text);
        }
        return json{};
    } catch (const TgBot::TgException& e) {
        printf("Error retrieving metadata: %s\n", e.what());
        return json{};
    }
}

int64_t ftes::TelegramApiFacade::getChatId() const {
    std::ifstream ifs(chat_id_file_);
    if (!ifs) {
        return 0;
    }
    int64_t chat_id;
    ifs >> chat_id;
    ifs.close();
    return chat_id;
}