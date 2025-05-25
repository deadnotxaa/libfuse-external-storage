add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
        PUBLIC ${FUSE_LIBRARIES}
        PUBLIC CLI11::CLI11
        PUBLIC fuse-filesystem
        PUBLIC fuse-args-parser
        PUBLIC telegram-api-facade
        PUBLIC telegram-external-storage
)

target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}
        PUBLIC ${FUSE_INCLUDE_DIRS}
)