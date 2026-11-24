qt_add_executable(tst_editor
    tst_editor.cpp
)

target_link_libraries(tst_editor PRIVATE
    Qt6::Test
    Qt6::Widgets
    bb_editor
    bb_document
    bb_core
)

add_test(NAME tst_editor COMMAND tst_editor)
set_tests_properties(tst_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_sql_preview
    tst_sql_preview.cpp
)

target_link_libraries(tst_sql_preview PRIVATE
    Qt6::Test
    Qt6::Widgets
    bb_editor
    bb_sqlgen
    bb_document
    bb_core
)

add_test(NAME tst_sql_preview COMMAND tst_sql_preview)
set_tests_properties(tst_sql_preview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_dialect_switch
    tst_dialect_switch.cpp
)

target_link_libraries(tst_dialect_switch PRIVATE
    Qt6::Test
    Qt6::Widgets
    bb_editor
    bb_sqlgen
    bb_document
    bb_core
)

add_test(NAME tst_dialect_switch COMMAND tst_dialect_switch)
set_tests_properties(tst_dialect_switch PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")