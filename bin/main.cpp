#include <iostream>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include "lib/parser/parser.hpp"
#include "lib/fuse-filesystem/fuse-filesystem.hpp"
#include "lib/telegram-external-storage/telegram-external-storage.hpp"
#include "lib/telegram-api/telegram-api.hpp"

namespace fes = fuse_external_storage;
namespace ftes = fuse_telegram_external_storage;

int main(const int argc, char** argv) {
    fes::ParserInterface* parser = new fes::FuseArgsParser("libfuse-telegram-storage-args-parser");

    auto [fuse_argc, fuse_argv, mount_point] = parser->Parse(argc, argv);

    const auto operations = fes::FuseFilesystem::getOperations();

    auto* state = new fes::FuseState{
        .mount_path = mount_point,
        .storage_interface =
            std::make_unique<ftes::TelegramExternalStorage>(""),
    };

    const int fuse_return = fuse_main(fuse_argc, fuse_argv, &operations, state);

    delete state;
    return fuse_return;
}
