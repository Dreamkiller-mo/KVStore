# src/kvstore/core/wal/CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    absl
    GIT_REPOSITORY git@github.com:abseil/abseil-cpp.git
    GIT_TAG 20260817.0
)
FetchContent_MakeAvailable(absl)

add_library(wal
    crc32c.cc
    encoder.cc
    file_committer.cc
    wal_codec.cc
    wal_manager.cc
    wal_record.cc
)

target_compile_features(wal PRIVATE cxx_std_23)

target_include_directories(wal
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/kvstore/core/wal
)

target_link_libraries(wal
    PRIVATE
        absl::crc32c
        absl::strings
        spdlog::spdlog
)  
