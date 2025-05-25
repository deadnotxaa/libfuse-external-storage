include(FetchContent)

# Telegram Bot API library
FetchContent_Declare(
        TgBot
        GIT_REPOSITORY https://github.com/reo7sp/tgbot-cpp.git
        GIT_TAG v1.9
)
FetchContent_MakeAvailable(TgBot)

# CLI parser
FetchContent_Declare(
        cli11_proj
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11_proj)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64 -Wall -lfuse3")
