# src/CMakeLists.txt

add_library(kvstore
    kvstore/core/wal/crc32c.cc
    kvstore/core/wal/encoder.cc
    kvstore/core/wal/wal_codec.cc
    kvstore/core/wal/wal_manager.cc
    kvstore/core/wal/wal_record.cc
)

target_include_directories(kvstore
    # Provide the required header dependencies to external parties
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include   
    
    # Internally relies on the corresponding implementation files
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(kvstore
    PRIVATE
        absl::crc32c
)