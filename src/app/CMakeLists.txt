# Editor widgets shared by the app and its tests.
qt_add_library(bb_editor STATIC
    ColumnEditorDialog.cpp
    ColumnEditorDialog.h
    EntityEditorPanel.cpp
    EntityEditorPanel.h
    SettingsDialog.cpp
    SettingsDialog.h
    Theme.cpp
    Theme.h
)

# Dark themes (QSS + icons) vendored from github.com/keshav-sahu7/qt-dark-theme.
set(BB_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../resources)
file(GLOB BB_THEME_QSS CONFIGURE_DEPENDS ${BB_RESOURCES}/themes/*.qss)
file(GLOB BB_THEME_ICONS CONFIGURE_DEPENDS ${BB_RESOURCES}/icons/*/*.png)
qt_add_resources(bb_editor "themes"
    PREFIX "/"
    BASE ${BB_RESOURCES}/themes
    FILES ${BB_THEME_QSS}
)
qt_add_resources(bb_editor "theme_icons"
    PREFIX "/"
    BASE ${BB_RESOURCES}
    FILES ${BB_THEME_ICONS}
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