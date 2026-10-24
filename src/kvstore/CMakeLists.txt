# src/kvstore/CMakeLists.txt

add_library(kvstore STATIC
    api/kvstore.cc
    storage/file.cc
    composition/composition_root.cc
    core/memtable/mem_record.cc
    core/memtable/skip_memtable.cc
    core/sstable/sstable.cc
    core/sstable/sstable_builder.cc
    core/sstable/sstable_codec.cc
)

add_library(kvstore_common STATIC
    common/codec.cc
)

target_include_directories(kvstore_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(kvstore_common PRIVATE cxx_std_23)

target_include_directories(kvstore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(kvstore PRIVATE cxx_std_23)

# wal library
add_subdirectory(core/wal/)
add_subdirectory(core/memtable)

target_link_libraries(kvstore PRIVATE wal)
target_link_libraries(kvstore PRIVATE kvstore_common)
