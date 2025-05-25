cmake_minimum_required(VERSION 3.31)

project(
        libfuse_external_storage
        VERSION 0.1
        DESCRIPTION "Custom userspace storage using FUSE"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

include(cmake/ExternalLibraries.cmake)

add_subdirectory(bin)
add_subdirectory(lib)
