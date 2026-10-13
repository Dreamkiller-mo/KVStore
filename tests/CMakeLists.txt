add_executable(skiplist_test
    unit/core/memtable/skiplist_test.cpp
)

target_link_libraries(skiplist_test PRIVATE kvstore)
target_compile_features(skiplist_test PRIVATE cxx_std_20)

add_test(NAME skiplist_test COMMAND skiplist_test)
