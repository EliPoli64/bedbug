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