add_library(fuse-filesystem fuse-filesystem.hpp fuse-filesystem.cpp)

target_link_libraries(fuse-filesystem
    PUBLIC external-storage-interface
    PRIVATE nlohmann_json::nlohmann_json
)

target_include_directories(fuse-filesystem
    PUBLIC ${PROJECT_SOURCE_DIR}
)
