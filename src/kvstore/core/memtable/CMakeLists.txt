# kvstore/core/memtable
add_library(memtable 
    mem_record.cc
    skip_memtable.cc
)

target_compile_features(memtable PRIVATE cxx_std_23)

target_include_directories(memtable
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/kvstore/core/memtable
)

target_link_libraries(memtable
    PUBLIC 
        spdlog::spdlog
)
