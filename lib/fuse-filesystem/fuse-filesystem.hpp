#ifndef FUSE_FILESYSTEM_HPP
#define FUSE_FILESYSTEM_HPP

#include <filesystem>
#include <memory>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include "lib/external-storage-interface.hpp"

namespace fuse_external_storage {

struct FuseState {
    std::filesystem::path mount_path;
    std::unique_ptr<ExternalStorageInterface> storage_interface;
};

class FuseFilesystem {
public:
    static int ff_getattr(const char*, struct stat*, fuse_file_info*);
    static int ff_readdir(const char*, void*, fuse_fill_dir_t, off_t, fuse_file_info*, fuse_readdir_flags);
    static int ff_open(const char*, fuse_file_info*);
    static int ff_read(const char*, char*, size_t, off_t, fuse_file_info*);
    static int ff_write(const char*, const char*, size_t, off_t, fuse_file_info*);
    static int ff_create(const char*, mode_t, fuse_file_info*);
    static int ff_unlink(const char*);
    static int ff_rename(const char*, const char*, unsigned int flags);
    static int ff_mkdir(const char*, mode_t);
    static int ff_rmdir(const char*);

    [[nodiscard]] static const fuse_operations& getOperations() {
        return operations_;
    }

private:
    // inline static const std::string fuse_directory_name_ = "fuse-external-fs";

    static std::pair<std::filesystem::path, int> getFullCurrentPath(const char*, const FuseState*);

    static constexpr fuse_operations operations_ = {
        .getattr    = ff_getattr,
        .mkdir      = ff_mkdir,
        .unlink     = ff_unlink,
        .rmdir      = ff_rmdir,
        .rename     = ff_rename,
        .open       = ff_open,
        .read       = ff_read,
        .write      = ff_write,
        .readdir    = ff_readdir,
        .create     = ff_create,
    };
};

} // fuse_external_storage

#endif //FUSE_FILESYSTEM_HPP
