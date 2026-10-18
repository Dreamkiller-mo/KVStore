# src/kvstore/CMakeLists.txt

add_library(kvstore STATIC
    api/kvstore.cc
    storage/file.cc
    composition/composition_root.cc
    core/memtable/mem_record.cc
    core/memtable/skip_memtable.cc
)

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
