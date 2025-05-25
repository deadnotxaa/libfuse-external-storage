#ifndef PARSER_INTERFACE_HPP
#define PARSER_INTERFACE_HPP

#include <tuple>

namespace fuse_external_storage {

class ParserInterface {
public:
    virtual std::tuple<int, char**, std::string> Parse(int, char**) = 0; // TODO: add result type instead of tuple

    virtual ~ParserInterface() = default;
};

} // fuse_external_storage

#endif //PARSER_INTERFACE_HPP
