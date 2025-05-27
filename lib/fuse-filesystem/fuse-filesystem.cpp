#include "fuse-filesystem.hpp"
#include <iostream>
#include <cstring>

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
    std::cerr << "[ff_getattr] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (std::string_view(path) == "/") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;

        return 0;
    }

    if (std::string_view(path) == "/" + fuse_directory_name_) {
        stbuf->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP | S_IROTH | S_IXOTH;
        stbuf->st_nlink = 2;

        return 0;
    }

    try {
        *stbuf = state->storage_interface->getAttr(current_path);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ff_getattr] Error: " << e.what() << std::endl;
        return -ENOENT;
    }
}

int fes::FuseFilesystem::ff_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info* fi, fuse_readdir_flags flags) {
    std::cerr << "[ff_readdir] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
    filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

    if (std::string_view(path) == "/") {
        struct stat stbuf{};
        memset(&stbuf, 0, sizeof(struct stat));

        filler(buf, fuse_directory_name_.c_str(), &stbuf, offset, static_cast<fuse_fill_dir_flags>(0));
    }

    try {
        auto entries = state->storage_interface->listDir(current_path);
        for (const auto& entry : entries) {
            struct stat st = {};
            st.st_mode = entry.is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
            st.st_nlink = entry.is_dir ? 2 : 1;
            st.st_size = entry.size;
            st.st_ctime = entry.ctime;
            st.st_mtime = entry.mtime;
            std::string name = std::filesystem::path(entry.path).filename().string();
            filler(buf, name.c_str(), &st, 0, static_cast<fuse_fill_dir_flags>(0));
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ff_readdir] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_open(const char* path, fuse_file_info* fi) {
    std::cerr << "[ff_open] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        struct stat st = state->storage_interface->getAttr(current_path);
        if (S_ISDIR(st.st_mode)) {
            return -EISDIR;
        }
        return 0;
    } catch (const std::exception& e) {
        return -ENOENT;
    }
}

int fes::FuseFilesystem::ff_read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) {
    std::cerr << "[ff_read] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->readFile(current_path, buf, size, offset);
    } catch (const std::exception& e) {
        std::cerr << "[ff_read] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_write(const char* path, const char* buf, size_t size, off_t offset, fuse_file_info* fi) {
    std::cerr << "[ff_write] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->writeFile(current_path, buf, size, offset);
    } catch (const std::exception& e) {
        std::cerr << "[ff_write] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_create(const char* path, mode_t mode, fuse_file_info* fi) {
    std::cerr << "[ff_create] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->createFile(current_path, mode);
    } catch (const std::exception& e) {
        std::cerr << "[ff_create] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_unlink(const char* path) {
    std::cerr << "[ff_unlink] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->unlinkFile(current_path);
    } catch (const std::exception& e) {
        std::cerr << "[ff_unlink] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_rename(const char* path_from, const char* path_to, unsigned int flags) {
    std::cerr << "[ff_rename] " << path_from << " to " << path_to << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path_from, error_from] = getFullCurrentPath(path_from, state);
    if (error_from) {
        return error_from;
    }
    auto [current_path_to, error_to] = getFullCurrentPath(path_to, state);
    if (error_to) {
        return error_to;
    }

    try {
        return state->storage_interface->rename(current_path_from, current_path_to);
    } catch (const std::exception& e) {
        std::cerr << "[ff_rename] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_mkdir(const char* path, mode_t mode) {
    std::cerr << "[ff_mkdir] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->createDir(current_path, mode);
    } catch (const std::exception& e) {
        std::cerr << "[ff_mkdir] Error: " << e.what() << std::endl;
        return -EIO;
    }
}

int fes::FuseFilesystem::ff_rmdir(const char* path) {
    std::cerr << "[ff_rmdir] " << path << std::endl;
    auto* state = static_cast<FuseState*>(fuse_get_context()->private_data);
    auto [current_path, error] = getFullCurrentPath(path, state);
    if (error) {
        return error;
    }

    try {
        return state->storage_interface->removeDir(current_path);
    } catch (const std::exception& e) {
        std::cerr << "[ff_rmdir] Error: " << e.what() << std::endl;
        return -EIO;
    }
}