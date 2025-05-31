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

// Debug the getAttr method to ensure it properly handles paths:
struct stat ftes::TelegramExternalStorage::getAttr(std::filesystem::path& path) {
    struct stat stbuf = {};
    json metadata = getMetadata();

    // Handle root directory
    if (path == "/" || path == "." || path.empty()) {
        stbuf.st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP | S_IROTH | S_IXOTH;
        stbuf.st_nlink = 2;
        return stbuf;
    }

    // Normalize path for lookup
    std::string normalized_path = path.string();
    if (normalized_path[0] != '/') {
        normalized_path = "/" + normalized_path;
    }

    const FileInfo* info = findFileInfo(normalized_path, metadata);

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

    // Debug output
    std::cerr << "[listDir] Listing directory: " << path << std::endl;
    std::cerr << "[listDir] Metadata contains " << metadata["files"].size() << " files" << std::endl;

    // Normalize the path for comparison
    std::string target_path = path.string();
    if (target_path.empty() || target_path == ".") {
        target_path = "/";
    } else if (target_path[0] != '/') {
        target_path = "/" + target_path;
    }

    // If not ending with '/', add it for directory comparison
    std::string dir_path = target_path;
    if (dir_path != "/" && dir_path.back() != '/') {
        dir_path += '/';
    }

    // Process files array
    if (metadata.contains("files") && metadata["files"].is_array()) {
        for (const auto& file_entry : metadata["files"]) {
            if (!file_entry.contains("path") || !file_entry["path"].is_string()) {
                std::cerr << "[listDir] Invalid file entry in metadata" << std::endl;
                continue;
            }

            std::string file_path = file_entry["path"].get<std::string>();
            std::cerr << "[listDir] Checking file: " << file_path << std::endl;

            // Skip entry if path is malformed
            if (file_path.empty() || file_path[0] != '/') {
                continue;
            }

            // Extract filename and parent directory
            std::filesystem::path p(file_path);
            std::string parent_dir = p.parent_path().string();
            if (parent_dir.empty()) parent_dir = "/";

            // Check if this file belongs in the requested directory
            if (parent_dir == target_path) {
                FileInfo info;
                info.path = file_path;
                info.message_id = file_entry["message_id"].get<int64_t>();
                info.size = file_entry["size"].get<size_t>();
                info.is_dir = file_entry["is_dir"].get<bool>();
                info.ctime = file_entry["ctime"].get<time_t>();
                info.mtime = file_entry["mtime"].get<time_t>();
                entries.push_back(info);
                std::cerr << "[listDir] Added file: " << file_path << " (parent: " << parent_dir << ")" << std::endl;
            }
        }
    }

    std::cerr << "[listDir] Returning " << entries.size() << " entries" << std::endl;
    return entries;
}

int ftes::TelegramExternalStorage::createFile(const std::filesystem::path& path, mode_t mode) {
    try {
        json metadata = getMetadata();

        // First remove any existing file with the same path
        removeFileInfo(path, metadata);

        // Now add the new file info
        time_t now = time(nullptr);
        json file_info = {
            {"path", path.string().empty() || path.string()[0] != '/' ? "/" + path.string() : path.string()},
            {"message_id", 0},
            {"size", 0},
            {"is_dir", false},
            {"ctime", now},
            {"mtime", now}
        };

        // Add to metadata and update
        metadata["files"].push_back(file_info);
        updateMetadata(metadata);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[createFile] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int ftes::TelegramExternalStorage::readFile(const std::filesystem::path& path, char* buf, size_t size, off_t offset) {
    json metadata = getMetadata();
    FileInfo* info = findFileInfo(path, metadata);

    if (!info) {
        return -ENOENT;
    }

    // Empty files (with message_id = 0) have no content to read
    if (info->message_id == 0) {
        return 0;  // EOF for empty files
    }

    // Check if offset is beyond file size
    if (offset >= static_cast<off_t>(info->size)) {
        return 0;  // EOF when offset is beyond file size
    }

    std::string temp_file = "/tmp/fuse_telegram_" + std::to_string(time(nullptr));

    if (!api_.downloadFile(info->message_id, temp_file)) {
        std::cerr << "[readFile] Failed to download file: " << path << std::endl;
        return -EIO;
    }

    // Verify the file exists and get its actual size
    struct stat st;
    if (stat(temp_file.c_str(), &st) != 0) {
        std::cerr << "[readFile] Failed to stat temp file" << std::endl;
        std::filesystem::remove(temp_file);
        return -EIO;
    }

    // Update size in metadata if it doesn't match actual file size
    if (static_cast<size_t>(st.st_size) != info->size) {
        json updated_metadata = metadata;
        removeFileInfo(path, updated_metadata);
        addFileInfo(path, info->message_id, st.st_size, false, updated_metadata);
        updateMetadata(updated_metadata);
    }

    std::ifstream ifs(temp_file, std::ios::binary);
    if (!ifs) {
        std::cerr << "[readFile] Failed to open temp file" << std::endl;
        std::filesystem::remove(temp_file);
        return -EIO;
    }

    // Calculate how many bytes we can actually read
    size_t bytes_available = st.st_size - offset;
    size_t bytes_to_read = std::min(size, bytes_available);

    ifs.seekg(offset);
    ifs.read(buf, bytes_to_read);
    size_t bytes_read = ifs.gcount();
    ifs.close();

    std::filesystem::remove(temp_file);
    return static_cast<int>(bytes_read);
}

int ftes::TelegramExternalStorage::writeFile(const std::filesystem::path& path, const char* buf, size_t size, off_t offset) {
    try {
        json metadata = getMetadata();
        FileInfo* info = findFileInfo(path, metadata);

        if (!info) {
            return -ENOENT;
        }

        // Create a temp file for content
        std::string temp_file = "/tmp/fuse_telegram_" + std::to_string(time(nullptr));

        // Check if this is an existing file with content
        if (info->message_id != 0) {
            // Download existing content
            if (!api_.downloadFile(info->message_id, temp_file)) {
                std::cerr << "[writeFile] Failed to download file: " << path << std::endl;
                return -EIO;
            }
        } else {
            // Create an empty file for new writes
            std::ofstream ofs(temp_file, std::ios::binary);
            if (!ofs) {
                std::cerr << "[writeFile] Failed to create temp file" << std::endl;
                return -EIO;
            }
            ofs.close();
        }

        // Update file content at specified offset
        std::fstream fs(temp_file, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) {
            std::filesystem::remove(temp_file);
            return -EIO;
        }

        // Get current file size
        fs.seekp(0, std::ios::end);
        size_t current_size = fs.tellp();

        // If offset is beyond current size, pad with zeros
        if (offset > current_size) {
            fs.seekp(current_size);
            std::vector<char> padding(offset - current_size, 0);
            fs.write(padding.data(), padding.size());
        }

        // Write the new data
        fs.seekp(offset);
        fs.write(buf, static_cast<std::streamsize>(size));
        fs.close();

        // Calculate new file size
        size_t new_size = std::max(static_cast<size_t>(offset) + size, current_size);

        // Upload new version
        int64_t new_message_id = api_.sendFile(temp_file, path.filename().string());
        if (new_message_id <= 0) {
            std::filesystem::remove(temp_file);
            return -EIO;
        }

        // Delete old message if it exists
        if (info->message_id > 0) {
            api_.deleteMessage(info->message_id);
        }

        // Update metadata - completely remove old entries and add new one
        removeFileInfo(path, metadata);

        // Normalize path for consistency
        std::string path_str = path.string();
        if (path_str.empty() || path_str == ".") {
            path_str = "/";
        } else if (path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        // Add updated file info
        time_t now = time(nullptr);
        metadata["files"].push_back({
            {"path", path_str},
            {"message_id", new_message_id},
            {"size", new_size},
            {"is_dir", false},
            {"ctime", info->ctime},
            {"mtime", now}
        });

        updateMetadata(metadata);
        std::filesystem::remove(temp_file);
        return static_cast<int>(size);
    } catch (const std::exception& e) {
        std::cerr << "[writeFile] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int ftes::TelegramExternalStorage::unlinkFile(const std::filesystem::path& path) {
    try {
        json metadata = getMetadata();
        FileInfo* info = findFileInfo(path, metadata);

        if (!info || info->is_dir) {
            return -ENOENT;
        }

        // Delete the message from Telegram if it has been uploaded
        if (info->message_id > 0) {
            if (!api_.deleteMessage(info->message_id)) {
                std::cerr << "[unlinkFile] Failed to delete message: " << info->message_id << std::endl;
                // Continue anyway to clean up metadata
            }
        }

        // Remove all entries with this path
        removeFileInfo(path, metadata);
        updateMetadata(metadata);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[unlinkFile] Error: " << e.what() << std::endl;
        return -EIO;
    }
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

// Fix the findFileInfo to handle path normalization better
ftes::FileInfo* ftes::TelegramExternalStorage::findFileInfo(const std::filesystem::path& path,
                                                         nlohmann::json& metadata) const {
    // Normalize path
    std::string path_str = path.string();
    if (path_str.empty() || path_str == ".") {
        path_str = "/";
    } else if (path_str[0] != '/') {
        path_str = "/" + path_str;
    }

    std::cerr << "[findFileInfo] Looking for: " << path_str << std::endl;

    // Check if files array exists
    if (!metadata.contains("files") || !metadata["files"].is_array()) {
        std::cerr << "[findFileInfo] No files array in metadata" << std::endl;
        return nullptr;
    }

    // Find matching entry (return the one with a message_id if there are duplicates)
    static FileInfo info;
    FileInfo* result = nullptr;

    for (auto& file : metadata["files"]) {
        if (file["path"] == path_str) {
            // If we already found an entry but this one has a message_id, prefer this one
            int64_t message_id = file["message_id"].get<int64_t>();

            if (!result || (message_id > 0 && result->message_id == 0)) {
                info.path = file["path"].get<std::string>();
                info.message_id = message_id;
                info.size = file["size"].get<size_t>();
                info.is_dir = file["is_dir"].get<bool>();
                info.ctime = file["ctime"].get<time_t>();
                info.mtime = file["mtime"].get<time_t>();
                result = &info;

                // If this entry has a message_id > 0, prefer it and stop searching
                if (message_id > 0) {
                    break;
                }
            }
        }
    }

    if (result) {
        std::cerr << "[findFileInfo] Found file: " << path_str
                  << " (message_id: " << result->message_id
                  << ", size: " << result->size << ")" << std::endl;
    } else {
        std::cerr << "[findFileInfo] File not found: " << path_str << std::endl;
    }

    return result;
}

void ftes::TelegramExternalStorage::addFileInfo(const std::filesystem::path& path,
                                              int64_t message_id,
                                              size_t size,
                                              bool is_dir,
                                              nlohmann::json& metadata) const {
    // Normalize the path
    std::string path_str = path.string();
    if (path_str.empty() || path_str == ".") {
        path_str = "/";
    } else if (path_str[0] != '/') {
        path_str = "/" + path_str;
    }

    // Make sure we have a files array
    if (!metadata.contains("files")) {
        metadata["files"] = json::array();
    }

    // Create the file info object
    json file_info = {
        {"path", path_str},
        {"message_id", message_id},
        {"size", size},
        {"is_dir", is_dir},
        {"ctime", time(nullptr)},
        {"mtime", time(nullptr)}
    };

    // Add to the files array
    metadata["files"].push_back(file_info);

    std::cerr << "[addFileInfo] Added file to metadata: " << path_str
              << " (message_id: " << message_id << ", size: " << size << ", is_dir: " << is_dir << ")" << std::endl;
}

void ftes::TelegramExternalStorage::removeFileInfo(const std::filesystem::path& path, nlohmann::json& metadata) const {
    // Normalize path
    std::string path_str = path.string();
    if (path_str.empty() || path_str == ".") {
        path_str = "/";
    } else if (path_str[0] != '/') {
        path_str = "/" + path_str;
    }

    std::cerr << "[removeFileInfo] Removing: " << path_str << std::endl;

    if (!metadata.contains("files") || !metadata["files"].is_array()) {
        return;
    }

    // Use erase-remove idiom to remove all entries with the matching path
    auto& files = metadata["files"];
    files.erase(
        std::remove_if(files.begin(), files.end(),
            [&path_str](const auto& file) {
                return file.contains("path") && file["path"] == path_str;
            }),
        files.end()
    );

    std::cerr << "[removeFileInfo] Metadata now contains " << metadata["files"].size() << " files" << std::endl;
}