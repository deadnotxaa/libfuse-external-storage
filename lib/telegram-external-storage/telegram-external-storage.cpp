#include "telegram-external-storage.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ftes = fuse_telegram_external_storage;
using json = nlohmann::json;

ftes::TelegramExternalStorage::TelegramExternalStorage(const std::string& api_token)
    : api_(api_token), bot_thread_([this] { api_.longPollThread(); }) {
}

ftes::TelegramExternalStorage::~TelegramExternalStorage() {
    if (bot_thread_.joinable()) {
        bot_thread_.join();
    }
}

struct stat ftes::TelegramExternalStorage::getAttr(std::filesystem::path& path) {
    struct stat stbuf = {};
    json metadata = getMetadata();

    if (path == "/") {
        stbuf.st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP | S_IROTH | S_IXOTH;
        stbuf.st_nlink = 2;
        return stbuf;
    }

    const FileInfo* info = findFileInfo(path, metadata);

    if (!info) {
        throw std::runtime_error("File not found");
    }

    stbuf.st_mode = info->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    stbuf.st_nlink = info->is_dir ? 2 : 1;

    stbuf.st_size = static_cast<__off64_t>(info->size);
    stbuf.st_ctime = info->ctime;
    stbuf.st_mtime = info->mtime;

    return stbuf;
}

std::vector<ftes::FileInfo> ftes::TelegramExternalStorage::listDir(const std::filesystem::path& path) {
    json metadata = getMetadata();
    std::vector<FileInfo> entries;

    std::string target_path = path == "/" ? "/" : path.string();
    if (target_path != "/") {
        target_path = "/" + target_path;
    }

    for (const auto& file : metadata["files"]) {
        auto file_path = file["path"].get<std::string>();
        if (std::filesystem::path(file_path).parent_path() == target_path) {
            entries.push_back({
                .path = file_path,
                .message_id = file["message_id"].get<int64_t>(),
                .ctime = file["ctime"].get<time_t>(),
                .mtime = file["mtime"].get<time_t>(),
                .size = file["size"].get<size_t>(),
                .is_dir = file["is_dir"].get<bool>()
            });
        }
    }

    return entries;
}

int ftes::TelegramExternalStorage::createFile(const std::filesystem::path& path, mode_t mode) {
    json metadata = getMetadata();

    if (findFileInfo(path, metadata)) {
        return -EEXIST;
    }

    // Create a temporary file to upload
    std::string temp_file = "/tmp/fuse_telegram_" + std::to_string(time(nullptr));
    std::ofstream ofs(temp_file, std::ios::binary);
    ofs.close();

    int64_t message_id = api_.sendFile(temp_file, path.filename().string());
    std::filesystem::remove(temp_file);

    addFileInfo(path, message_id, 0, false, metadata);
    updateMetadata(metadata);
    return 0;
}

int ftes::TelegramExternalStorage::readFile(const std::filesystem::path& path, char* buf, size_t size, off_t offset) {
    json metadata = getMetadata();
    FileInfo* info = findFileInfo(path, metadata);

    if (!info) {
        return -ENOENT;
    }

    std::string temp_file = "/tmp/fuse_telegram_" + std::to_string(time(nullptr));

    if (!api_.downloadFile(info->message_id, temp_file)) {
        return -EIO;
    }

    std::ifstream ifs(temp_file, std::ios::binary);
    if (!ifs) {
        std::filesystem::remove(temp_file);
        return -EIO;
    }

    ifs.seekg(offset);
    ifs.read(buf, static_cast<std::streamsize>(size));
    size_t bytes_read = ifs.gcount();
    ifs.close();

    std::filesystem::remove(temp_file);

    return static_cast<int>(bytes_read);
}

int ftes::TelegramExternalStorage::writeFile(const std::filesystem::path& path, const char* buf, size_t size, off_t offset) {
    json metadata = getMetadata();
    FileInfo* info = findFileInfo(path, metadata);

    if (!info) {
        return -ENOENT;
    }

    // Download existing file
    std::string temp_file = "/tmp/fuse_telegram_" + std::to_string(time(nullptr));

    if (!api_.downloadFile(info->message_id, temp_file)) {
        return -EIO;
    }

    // Update file content
    std::fstream fs(temp_file, std::ios::in | std::ios::out | std::ios::binary);
    if (!fs) {
        std::filesystem::remove(temp_file);
        return -EIO;
    }

    fs.seekp(offset);
    fs.write(buf, static_cast<std::streamsize>(size));
    size_t new_size = std::max(info->size, static_cast<size_t>(offset) + size);
    fs.close();

    // Upload new file
    int64_t new_message_id = api_.sendFile(temp_file, path.filename().string());
    api_.deleteMessage(info->message_id);

    // Update metadata
    removeFileInfo(path, metadata);
    addFileInfo(path, new_message_id, new_size, false, metadata);
    updateMetadata(metadata);

    std::filesystem::remove(temp_file);

    return static_cast<int>(size);
}

int ftes::TelegramExternalStorage::unlinkFile(const std::filesystem::path& path) {
    json metadata = getMetadata();
    const FileInfo* info = findFileInfo(path, metadata);

    if (!info || info->is_dir) {
        return -ENOENT;
    }

    if (!api_.deleteMessage(info->message_id)) {
        return -EIO;
    }

    removeFileInfo(path, metadata);
    updateMetadata(metadata);

    return 0;
}

int ftes::TelegramExternalStorage::createDir(const std::filesystem::path& path, mode_t mode) {
    json metadata = getMetadata();
    if (findFileInfo(path, metadata)) {
        return -EEXIST;
    }

    addFileInfo(path, 0, 0, true, metadata);
    updateMetadata(metadata);

    return 0;
}

int ftes::TelegramExternalStorage::removeDir(const std::filesystem::path& path) {
    json metadata = getMetadata();
    const FileInfo* info = findFileInfo(path, metadata);

    if (!info || !info->is_dir) {
        return -ENOENT;
    }

    // Check if directory is empty
    const std::string target_path = path == "/" ? "/" : "/" + path.string();

    for (const auto& file : metadata["files"]) {
        if (std::filesystem::path(file["path"].get<std::string>()).parent_path() == target_path) {
            return -ENOTEMPTY;
        }
    }

    removeFileInfo(path, metadata);
    updateMetadata(metadata);

    return 0;
}

int ftes::TelegramExternalStorage::rename(const std::filesystem::path& from, const std::filesystem::path& to) {
    json metadata = getMetadata();
    const FileInfo* info = findFileInfo(from, metadata);

    if (!info) {
        return -ENOENT;
    }

    if (findFileInfo(to, metadata)) {
        return -EEXIST;
    }

    removeFileInfo(from, metadata);
    addFileInfo(to, info->message_id, info->size, info->is_dir, metadata);
    updateMetadata(metadata);

    return 0;
}

json ftes::TelegramExternalStorage::getMetadata() const {
    json metadata = api_.getMetadata();

    if (metadata.is_null()) {
        metadata = json{{"files", json::array()}};
    }

    return metadata;
}

void ftes::TelegramExternalStorage::updateMetadata(const json& metadata) const {
    [[maybe_unused]] auto result = api_.updateMetadata(metadata);
}

ftes::FileInfo* ftes::TelegramExternalStorage::findFileInfo(const std::filesystem::path& path, json& metadata) const {
    std::string target_path = path == "/" ? "/" : "/" + path.string();

    for (auto& file : metadata["files"]) {
        if (file["path"].get<std::string>() == target_path) {
            static FileInfo info;

            info.path = file["path"];
            info.message_id = file["message_id"];
            info.ctime = file["ctime"];
            info.mtime = file["mtime"];
            info.size = file["size"];
            info.is_dir = file["is_dir"];

            return &info;
        }
    }

    return nullptr;
}

void ftes::TelegramExternalStorage::addFileInfo(const std::filesystem::path& path, int64_t message_id, size_t size, bool is_dir, json& metadata) const {
    const json file_info = {
        {"path", path == "/" ? "/" : "/" + path.string()},
        {"message_id", message_id},
        {"ctime", time(nullptr)},
        {"mtime", time(nullptr)},
        {"size", size},
        {"is_dir", is_dir}
    };

    metadata["files"].push_back(file_info);
}

void ftes::TelegramExternalStorage::removeFileInfo(const std::filesystem::path& path, json& metadata) const {
    std::string target_path = path == "/" ? "/" : "/" + path.string();
    json new_files = json::array();

    for (const auto& file : metadata["files"]) {
        if (file["path"].get<std::string>() != target_path) {
            new_files.push_back(file);
        }
    }

    metadata["files"] = new_files;
}