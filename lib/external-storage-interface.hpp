#ifndef EXTERNAL_STORAGE_INTERFACE_HPP
#define EXTERNAL_STORAGE_INTERFACE_HPP

#include <filesystem>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

namespace fuse_external_storage {

class ExternalStorageInterface {
public:
    virtual struct stat getAttr(std::filesystem::path&) = 0;

    virtual ~ExternalStorageInterface() = default;
};

} // fuse_external_storage

#endif //EXTERNAL_STORAGE_INTERFACE_HPP
