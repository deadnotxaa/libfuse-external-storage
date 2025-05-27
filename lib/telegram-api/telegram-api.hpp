#ifndef TELEGRAM_API_HPP
#define TELEGRAM_API_HPP

#include <string>
#include <filesystem>

#include <tgbot/tgbot.h>
#include <nlohmann/json.hpp>

namespace fuse_telegram_external_storage {

struct FileInfo {
    std::string path;
    int64_t message_id;
    time_t ctime;
    time_t mtime;
    size_t size;
    bool is_dir;
};

class TelegramApiFacade {
public:
    explicit TelegramApiFacade(std::string api_token);

    // Initialize bot and start long polling
    void longPollThread() const;

    // Send a file to the chat and return the message ID
    int64_t sendFile(const std::filesystem::path& path, const std::string& file_name) const;

    // Download a file from a message ID to a local path
    bool downloadFile(int64_t message_id, const std::filesystem::path& dest_path) const;

    // Delete a message by ID
    bool deleteMessage(int64_t message_id) const;

    // Send or update the special metadata message
    int64_t updateMetadata(const nlohmann::json& metadata) const;

    // Get the current metadata message content
    nlohmann::json getMetadata() const;

    // Get chat ID from local file
    int64_t getChatId() const;

private:
    std::string api_token_;
    TgBot::Bot bot_;

    std::string chat_id_file_ = std::string(getenv("HOME")) + "/chat_id.txt";
    std::string metadata_message_file_ = std::string(getenv("HOME")) + "/metadata_message_id.txt";

    mutable int64_t metadata_message_id_ = 0;
};

} // fuse_telegram_external_storage

#endif //TELEGRAM_API_HPP
