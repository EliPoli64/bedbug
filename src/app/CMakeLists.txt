# Editor widgets shared by the app and its tests.
qt_add_library(bb_editor STATIC
    ColumnEditorDialog.cpp
    ColumnEditorDialog.h
    EntityEditorPanel.cpp
    EntityEditorPanel.h
    SettingsDialog.cpp
    SettingsDialog.h
)

target_include_directories(bb_editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_editor PUBLIC
    Qt6::Widgets
    bb_document
    bb_core
)

qt_add_executable(bedbug
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(bedbug PRIVATE
    Qt6::Widgets
    bb_canvas
    bb_document
    bb_core
    bb_editor
)