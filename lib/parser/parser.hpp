#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <tuple>

#include <CLI/CLI.hpp>

#include "lib/parser-interface.hpp"

namespace fuse_external_storage {

class FuseArgsParser final : public ParserInterface {
public:
    explicit FuseArgsParser(std::string parser_name);

    std::tuple<int, char**, std::string> Parse(int argc, char** argv) override;

private:
    static void filter_mount_point_args(int, char**, int&, char**&);

    CLI::App app_;
    std::string mount_point_;
};

} // fuse_external_storage

#endif //PARSER_HPP
