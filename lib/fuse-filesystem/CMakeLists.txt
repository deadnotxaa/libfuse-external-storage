add_library(fuse-filesystem fuse-filesystem.hpp fuse-filesystem.cpp)

target_link_libraries(fuse-filesystem
    PUBLIC external-storage-interface
)

target_include_directories(fuse-filesystem
    PUBLIC ${PROJECT_SOURCE_DIR}
)
