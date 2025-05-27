#include "telegram-api.hpp"

#include <utility>

namespace ftes = fuse_telegram_external_storage;
using json = nlohmann::json;

ftes::TelegramApiFacade::TelegramApiFacade(std::string api_token)
    : api_token_(std::move(api_token)), bot_(api_token_)
{
    // Set up the bot with the provided API token and add start command handler
    bot_.getEvents().onCommand("start", [this](const TgBot::Message::Ptr& message) { // TODO: Add authentication
        bot_.getApi().sendMessage(message->chat->id, "Welcome to FUSE Telegram External Storage Bot!");

        int64_t chat_id = message->chat->id;

        // Save the chat ID to a file
        std::ofstream chat_id_file(chat_id_file_, std::ios::out);

        if (chat_id_file.good()) {
            chat_id_file << chat_id;
            chat_id_file.close();

            [[maybe_unused]] auto result =
                bot_.getApi().sendMessage(chat_id, "Chat ID saved. Bot is ready!");
        } else {
            [[maybe_unused]] auto result =
                bot_.getApi().sendMessage(chat_id, "Error saving chat ID.");
        }
    });

    // Initialize metadata message ID
    try {
        auto metadata = getMetadata();

        if (!metadata.is_null()) {
            std::ifstream metadata_file(metadata_message_file_, std::ios::in);

            if (metadata_file) {
                metadata_file >> metadata_message_id_;
                metadata_file.close();
            }
        }
    } catch ([[maybe_unused]] const std::exception& e) {
        // No metadata message yet; will be created on first update
    }
}

int64_t ftes::TelegramApiFacade::sendFile(const std::filesystem::path& path, const std::string& file_name) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        throw std::runtime_error("Chat ID not set");
    }

    TgBot::InputFile::Ptr input_file = TgBot::InputFile::fromFile(path.string(), "application/octet-stream");
    const auto message = bot_.getApi().sendDocument(chat_id, input_file, file_name);

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

        ofs.write(file_content.data(), static_cast<std::streamsize>(file_content.size()));
        ofs.close();

        return true;
    } catch (const TgBot::TgException& e) {
        printf("Error downloading file: %s\n", e.what());
        return false;
    }
}

bool ftes::TelegramApiFacade::deleteMessage(const int64_t message_id) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return false;
    }

    try {
        return bot_.getApi().deleteMessage(chat_id, static_cast<int>(message_id));
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

    const std::string metadata_str = metadata.dump();

    try {
        const auto message = bot_.getApi().sendMessage(chat_id, metadata_str);
        const int64_t new_message_id = message->messageId;

        // Pin the new metadata message
        [[maybe_unused]] auto pin_result =
            bot_.getApi().pinChatMessage(chat_id, static_cast<int>(new_message_id), true);

        // Save new metadata message ID
        std::ofstream ofs(std::getenv("HOME") + std::string("/metadata_message_id.txt"));

        if (ofs) {
            ofs << new_message_id;
            ofs.close();
        }

        // Unpin and delete old metadata message if it exists
        if (metadata_message_id_ != 0) {
            [[maybe_unused]] auto unpin_result =
                bot_.getApi().unpinChatMessage(chat_id, static_cast<int>(metadata_message_id_));

            [[maybe_unused]] auto delete_result =
                bot_.getApi().deleteMessage(chat_id, static_cast<int>(metadata_message_id_));
        }
        metadata_message_id_ = new_message_id;

        return new_message_id;
    } catch (const TgBot::TgException& e) {
        printf("Error updating metadata: %s\n", e.what());
        throw;
    }
}

nlohmann::json ftes::TelegramApiFacade::getMetadata() const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return json{};
    }

    try {
        const auto chat = bot_.getApi().getChat(chat_id);

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
    std::ifstream chat_id_file(chat_id_file_);
    if (!chat_id_file) {
        return 0;
    }

    int64_t chat_id;
    chat_id_file >> chat_id;
    chat_id_file.close();

    return chat_id;
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
