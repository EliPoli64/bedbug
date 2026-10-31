qt_add_library(bb_document STATIC
    DocumentController.cpp
    DocumentController.h
    commands/MoveEntityNodeCommand.cpp
    commands/MoveEntityNodeCommand.h
    commands/ModelSnapshotCommand.cpp
    commands/ModelSnapshotCommand.h
)

target_include_directories(bb_document PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_document PUBLIC
    Qt6::Gui
    bb_core
)
