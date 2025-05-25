add_library(telegram-api-facade telegram-api.hpp telegram-api.cpp)

target_link_libraries(telegram-api-facade
        PUBLIC TgBot
)

target_include_directories(telegram-api-facade
        PUBLIC ${PROJECT_SOURCE_DIR}
)
