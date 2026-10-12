qt_add_executable(tst_postgres
    tst_postgres.cpp
)

target_link_libraries(tst_postgres PRIVATE
    Qt6::Test
    bb_sqlgen
)

add_test(NAME tst_postgres COMMAND tst_postgres)

qt_add_executable(tst_golden
    tst_golden.cpp
)

target_link_libraries(tst_golden PRIVATE
    Qt6::Test
    bb_sqlgen
    bb_core
)

add_test(NAME tst_golden COMMAND tst_golden)