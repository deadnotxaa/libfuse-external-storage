#include "fuse-filesystem.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace fes = fuse_external_storage;

std::pair<std::filesystem::path, int> fes::FuseFilesystem::getFullCurrentPath(const char* mounted_fs_path, const FuseState* state) {
    if (!state) {
        std::cerr << "[getFullCurrentPath] Error: FUSE state is null!" << std::endl;
        return {"", -EIO};
    }

    std::filesystem::path current_path = state->mount_path;
    if (std::string_view(mounted_fs_path) != "/") {
        current_path /= mounted_fs_path;
    }

    std::cerr << "[getFullCurrentPath] Current path: " << current_path << std::endl;
    std::cerr << "[getFullCurrentPath] Path in mounted fs: " << mounted_fs_path << std::endl;

    return {current_path, 0};
}

int fes::FuseFilesystem::ff_getattr(const char* path, struct stat* stbuf, fuse_file_info* fi) {
    std::cerr << "[ff_getattr]" << std::endl;
    const auto* current_fuse_state = static_cast<FuseState*>(fuse_get_context()->private_data);

    auto [current_path, error] = getFullCurrentPath(path, current_fuse_state);

    if (error) {
        return error;
    }

    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (std::string_view(path) == "/") {
        stbuf->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP | S_IROTH | S_IXOTH;
        stbuf->st_nlink = 2;

        return 0;
    }

    try {
        *stbuf = current_fuse_state->storage_interface->getAttr(current_path);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ff_getattr] Error: " << e.what() << std::endl;
        return -ENOENT;
    }

    return 0;
}

int fes::FuseFilesystem::ff_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info* fi, fuse_readdir_flags flags) {
    std::cerr << "[ff_readdir]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    // Necessary directories
    filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
    filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

    // TODO: Implement the logic to read directory contents from telegram


    return 0;
}

int fes::FuseFilesystem::ff_open(const char* path, fuse_file_info* fi) {
    std::cerr << "[ff_open]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    const int fd = ::open(current_path.c_str(), fi->flags);

    if (fd == -1) {
        return -errno;
    }
    ::close(fd);

    return 0;
}

int fes::FuseFilesystem::ff_read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) {
    std::cerr << "[ff_read]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    std::ifstream file(current_path, std::ios::binary);
    if (!file.good()) {
        return -errno;
    }

    file.seekg(offset);
    file.read(buf, static_cast<std::streamsize>(size));

    return static_cast<int>(file.gcount());
}

int fes::FuseFilesystem::ff_write(const char* path, const char* buf, size_t size, off_t offset, fuse_file_info* fi) {
    std::cerr << "[ff_write]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    std::fstream file(current_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.good()) {
        return -errno;
    }

    file.seekp(offset);
    file.write(buf, static_cast<std::streamsize>(size));

    return static_cast<int>(size);
}

int fes::FuseFilesystem::ff_create(const char* path, mode_t mode, fuse_file_info* fi) {
    std::cerr << "[ff_create]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    const int fd = ::open(current_path.c_str(), O_CREAT | O_WRONLY, mode);

    if (fd == -1) {
        return -errno;
    }
    ::close(fd);

    return 0;
}

int fes::FuseFilesystem::ff_unlink(const char* path) {
    std::cerr << "[ff_unlink]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    return std::filesystem::remove(current_path) ? 0 : -errno;
}

int fes::FuseFilesystem::ff_rename(const char* path_from, const char* path_to, unsigned int flags) {
    std::cerr << "[ff_rename]" << std::endl;
    auto [current_path_from, error_from] = getFullCurrentPath(path_from, static_cast<FuseState*>(fuse_get_context()->private_data));
    if (error_from) {
        return error_from;
    }

    auto [current_path_to, error_to] = getFullCurrentPath(path_to, static_cast<FuseState*>(fuse_get_context()->private_data));
    if (error_to) {
        return error_to;
    }

    const std::string absolute_path_from{current_path_from.string() + std::string(path_from)};
    const std::string absolute_path_to{current_path_from.string() + std::string(path_to)};

    const std::filesystem::path full_path_from = absolute_path_from;
    const std::filesystem::path full_path_to = absolute_path_to;

    std::error_code ec;
    std::filesystem::rename(full_path_from, full_path_to, ec);

    return ec ? -errno : 0;
}

int fes::FuseFilesystem::ff_mkdir(const char* path, mode_t mode) {
    std::cerr << "[ff_mkdir]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    std::error_code ec;
    std::filesystem::create_directory(current_path, ec);

    if (ec) {
        return -errno;
    }

    chmod(current_path.c_str(), mode);

    return 0;
}

int fes::FuseFilesystem::ff_rmdir(const char* path) {
    std::cerr << "[ff_rmdir]" << std::endl;
    auto [current_path, error] = getFullCurrentPath(path, static_cast<FuseState*>(fuse_get_context()->private_data));

    if (error) {
        return error;
    }

    return std::filesystem::remove(current_path) ? 0 : -errno;
}
