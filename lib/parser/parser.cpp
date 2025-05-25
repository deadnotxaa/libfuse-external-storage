#include "parser.hpp"

#include <cstring>
#include <filesystem>

namespace fes = fuse_external_storage;

fes::FuseArgsParser::FuseArgsParser(std::string parser_name)
    : app_(std::move(parser_name))
{
    app_.add_option(
        "-m,--mount-point",
        mount_point_,
        "Mount point of the filesystem"
    )
    ->required()
    ->check(CLI::ExistingDirectory);

    app_.add_option(
        "-d,--debug",
        "libfuse debug mode"
    );

    app_.add_option(
        "-f",
        "libfuse option"
    );

    app_.allow_extras();
}

std::tuple<int, char**, std::string> fes::FuseArgsParser::Parse(int argc, char **argv) {
    try {
        app_.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        [[maybe_unused]] int error_return = app_.exit(e);
        exit(EXIT_FAILURE);
    }

    int new_argc;
    char** new_argv;
    filter_mount_point_args(argc, argv, new_argc, new_argv);

    mount_point_ = std::filesystem::canonical(mount_point_);

    return std::make_tuple(new_argc, new_argv, mount_point_);
}

void fes::FuseArgsParser::filter_mount_point_args(int argc, char** argv, int& new_argc, char**& new_argv) {
    std::vector<char*> filtered_args;

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "-m") == 0 || std::strcmp(argv[i], "--mount-point") == 0) {
            continue; // Skip only the current argument
        }
        filtered_args.push_back(argv[i]);
    }

    new_argc = static_cast<int>(filtered_args.size());
    new_argv = new char*[new_argc];

    for (int i = 0; i < new_argc; ++i) {
        new_argv[i] = filtered_args[i];
    }
}
