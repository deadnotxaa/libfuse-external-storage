add_subdirectory(fuse-filesystem)
add_subdirectory(parser)
add_subdirectory(telegram-api)
add_subdirectory(telegram-external-storage)

add_library(external-storage-interface INTERFACE external-storage-interface.hpp)
add_library(parser-interface INTERFACE parser-interface.hpp)
