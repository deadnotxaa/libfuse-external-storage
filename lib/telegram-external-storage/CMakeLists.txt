add_library(telegram-external-storage telegram-external-storage.hpp telegram-external-storage.cpp)

target_link_libraries(telegram-external-storage
        PUBLIC telegram-api-facade
        PUBLIC ${FUSE_LIBRARIES}
        PUBLIC external-storage-interface
        PRIVATE nlohmann_json::nlohmann_json
)

target_include_directories(telegram-external-storage
        PUBLIC ${PROJECT_SOURCE_DIR}
        PUBLIC ${FUSE_INCLUDE_DIRS}
)
