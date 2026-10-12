qt_add_library(bb_canvas STATIC
    CrowsFootPainter.cpp
    CrowsFootPainter.h
    DiagramScene.cpp
    DiagramScene.h
    DiagramView.cpp
    DiagramView.h
    EntityItem.cpp
    EntityItem.h
    RelationshipItem.cpp
    RelationshipItem.h
)

target_include_directories(bb_canvas PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_canvas PUBLIC
    Qt6::Widgets
    bb_core
)