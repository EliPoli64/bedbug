qt_add_library(bb_core STATIC
    CanonicalType.cpp
    CanonicalType.h
    Column.h
    Entity.cpp
    Entity.h
    Id.cpp
    Id.h
    Index.h
    JsonSerializer.cpp
    JsonSerializer.h
    Model.cpp
    Model.h
    Relationship.h
    Version.cpp
    Version.h
)

target_include_directories(bb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_core PUBLIC
    Qt6::Core
)