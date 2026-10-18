# basic_usage示例程序
add_executable(basic_usage
    basic_usage.cc
)

target_link_libraries(basic_usage
    PRIVATE
        kvstore
        spdlog::spdlog
)

if(BUILD_TESTING)
    add_test(NAME basic_usage COMMAND basic_usage)
endif()
