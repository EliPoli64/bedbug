qt_add_executable(tst_document
    tst_document.cpp
)

target_link_libraries(tst_document PRIVATE
    Qt6::Test
    bb_document
    bb_core
)

add_test(NAME tst_document COMMAND tst_document)

qt_add_executable(tst_undo
    tst_undo.cpp
)

target_link_libraries(tst_undo PRIVATE
    Qt6::Test
    bb_document
    bb_core
)

add_test(NAME tst_undo COMMAND tst_undo)
