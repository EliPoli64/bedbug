# Editor widgets shared by the app and its tests.
qt_add_library(bb_editor STATIC
    ColumnEditorDialog.cpp
    ColumnEditorDialog.h
    DialectSelector.cpp
    DialectSelector.h
    EntityEditorPanel.cpp
    EntityEditorPanel.h
    SettingsDialog.cpp
    SettingsDialog.h
    SqlPreviewPanel.cpp
    SqlPreviewPanel.h
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
    bb_sqlgen
    bb_core
)

qt_add_executable(bedbug
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(bedbug PRIVATE
    Qt6::Widgets
    Qt6::Svg
    bb_canvas
    bb_document
    bb_core
    bb_editor
)

# App icon, rendered from the vector artwork at startup.
qt_add_resources(bedbug "branding"
    PREFIX "/assets"
    BASE "${CMAKE_SOURCE_DIR}/assets"
    FILES "${CMAKE_SOURCE_DIR}/assets/logo.svg"
)

if(WIN32)
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets/logo.ico" BEDBUG_ICON_PATH)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/bedbug.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/bedbug.rc"
        @ONLY
    )
    target_sources(bedbug PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bedbug.rc")
endif()

# Install and deploy rules power the CI release artifacts.
set_target_properties(bedbug PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "beDBug"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.bedbug.app"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

install(TARGETS bedbug
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

qt_generate_deploy_app_script(
    TARGET bedbug
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})