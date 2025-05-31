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
        // Try to get the message directly by ID
        TgBot::Message::Ptr message;
        try {
            message = bot_.getApi().forwardMessage(chat_id, chat_id, static_cast<int>(message_id), false);
            // Delete the forwarded message as we don't need it
            bot_.getApi().deleteMessage(chat_id, message->messageId);
        } catch (const TgBot::TgException& e) {
            std::cerr << "Could not forward message: " << e.what() << std::endl;
            // Fall back to searching in updates
            auto updates = bot_.getApi().getUpdates(0, 100);
            for (const auto& update : updates) {
                if (update->message && update->message->messageId == message_id && update->message->document) {
                    message = update->message;
                    break;
                }
            }
        }

        if (!message || !message->document) {
            // If we still don't have the message or it has no document, try to get message history
            std::cerr << "File not found for message ID: " << message_id << std::endl;
            return false;
        }

        // Get file path from Telegram
        std::string file_id = message->document->fileId;
        auto file = bot_.getApi().getFile(file_id);

        // Download file content as string
        std::string file_content = bot_.getApi().downloadFile(file->filePath);

        // Write content to destination file
        std::ofstream ofs(dest_path, std::ios::binary);

        if (!ofs) {
            std::cerr << "Failed to open destination file: " << dest_path.c_str() << std::endl;
            return false;
        }

        ofs.write(file_content.data(), static_cast<std::streamsize>(file_content.size()));
        ofs.close();

        return true;
    } catch (const TgBot::TgException& e) {
        std::cerr << "Error downloading file: " << e.what() << std::endl;
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

// In telegram-api.cpp, rewrite the updateMetadata method:
int64_t ftes::TelegramApiFacade::updateMetadata(const nlohmann::json& metadata) const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        throw std::runtime_error("Chat ID not set");
    }

    const std::string metadata_str = metadata.dump();

    // Create a temporary file for the metadata
    std::string temp_file = "/tmp/fuse_telegram_metadata_" + std::to_string(time(nullptr));
    std::ofstream ofs(temp_file);
    if (!ofs) {
        throw std::runtime_error("Failed to create temporary metadata file");
    }
    ofs << metadata_str;
    ofs.close();

    try {
        // Send metadata as a document with fixed name for easy identification
        TgBot::InputFile::Ptr input_file = TgBot::InputFile::fromFile(temp_file, "application/json");
        const auto message = bot_.getApi().sendDocument(chat_id, input_file, "metadata.json");
        const int64_t new_message_id = message->messageId;

        // Try to pin the new metadata message and handle errors
        try {
            bot_.getApi().pinChatMessage(chat_id, static_cast<int>(new_message_id), true);
        } catch (const TgBot::TgException& e) {
            printf("Warning: could not pin metadata message: %s\n", e.what());
            // Continue despite pin error
        }

        // Save new metadata message ID
        std::ofstream id_ofs(metadata_message_file_);
        if (id_ofs) {
            id_ofs << new_message_id;
            id_ofs.close();
        }

        // Try to unpin and delete old metadata message if it exists
        if (metadata_message_id_ != 0 && metadata_message_id_ != new_message_id) {
            try {
                bot_.getApi().unpinChatMessage(chat_id, static_cast<int>(metadata_message_id_));
            } catch (const TgBot::TgException& e) {
                printf("Warning: could not unpin old metadata message: %s\n", e.what());
                // Continue despite unpin error
            }

            try {
                bot_.getApi().deleteMessage(chat_id, static_cast<int>(metadata_message_id_));
            } catch (const TgBot::TgException& e) {
                printf("Warning: could not delete old metadata message: %s\n", e.what());
                // Continue despite delete error
            }
        }

        // Update the metadata message ID
        metadata_message_id_ = new_message_id;

        // Clean up temp file
        std::filesystem::remove(temp_file);
        return new_message_id;
    } catch (const TgBot::TgException& e) {
        std::filesystem::remove(temp_file);
        printf("Error updating metadata: %s\n", e.what());
        throw;
    }
}

// In telegram-api.cpp, update the getMetadata method:
nlohmann::json ftes::TelegramApiFacade::getMetadata() const {
    int64_t chat_id = getChatId();
    if (chat_id == 0) {
        return json{};
    }

    try {
        const auto chat = bot_.getApi().getChat(chat_id);

        if (chat->pinnedMessage && chat->pinnedMessage->document) {
            metadata_message_id_ = chat->pinnedMessage->messageId;

            // Download metadata file
            std::string temp_file = "/tmp/fuse_telegram_metadata_" + std::to_string(time(nullptr));
            std::string file_id = chat->pinnedMessage->document->fileId;
            auto file = bot_.getApi().getFile(file_id);
            std::string file_content = bot_.getApi().downloadFile(file->filePath);

            std::ofstream ofs(temp_file);
            ofs << file_content;
            ofs.close();

            // Parse the metadata
            std::ifstream ifs(temp_file);
            std::string metadata_str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            ifs.close();

            std::filesystem::remove(temp_file);

            return json::parse(metadata_str);
        }

        if (chat->pinnedMessage && !chat->pinnedMessage->text.empty()) {
            // Legacy support for text-based metadata
            metadata_message_id_ = chat->pinnedMessage->messageId;
            return json::parse(chat->pinnedMessage->text);
        }

        return json{{"files", json::array()}};
    } catch (const TgBot::TgException& e) {
        printf("Error retrieving metadata: %s\n", e.what());
        return json{{"files", json::array()}};
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
