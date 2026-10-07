# src/kvstore/CMakeLists.txt

add_library(kvstore STATIC
    api/kvstore.cc
    storage/file.cc
    composition/composition_root.cc
)

target_include_directories(kvstore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(kvstore PRIVATE cxx_std_20)

# wal library
add_subdirectory(core/wal/)

target_link_libraries(kvstore PRIVATE wal)
