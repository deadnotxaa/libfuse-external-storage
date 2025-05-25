add_library(fuse-args-parser parser.hpp parser.cpp)

target_link_libraries(fuse-args-parser
    PRIVATE CLI11::CLI11
    PUBLIC parser-interface
)

target_include_directories(fuse-args-parser
        PUBLIC ${PROJECT_SOURCE_DIR}
)
