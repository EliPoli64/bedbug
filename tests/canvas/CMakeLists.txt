qt_add_executable(tst_canvas
    tst_canvas.cpp
)

target_link_libraries(tst_canvas PRIVATE
    Qt6::Test
    Qt6::Widgets
    bb_canvas
    bb_document
    bb_core
)

add_test(NAME tst_canvas COMMAND tst_canvas)
set_tests_properties(tst_canvas PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")