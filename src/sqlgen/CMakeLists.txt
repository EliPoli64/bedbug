qt_add_library(bb_sqlgen STATIC
    SqlDialect.h
    SqlGenerator.cpp
    SqlGenerator.h
    dialects/Postgres.cpp
    dialects/Postgres.h
)

target_include_directories(bb_sqlgen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_sqlgen PUBLIC
    bb_core
)