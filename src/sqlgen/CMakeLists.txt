qt_add_library(bb_sqlgen STATIC
    DialectRegistry.cpp
    DialectRegistry.h
    SqlDialect.h
    SqlGenerator.cpp
    SqlGenerator.h
    dialects/MySql.cpp
    dialects/MySql.h
    dialects/Postgres.cpp
    dialects/Postgres.h
    dialects/SqlServer.cpp
    dialects/SqlServer.h
)

target_include_directories(bb_sqlgen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_sqlgen PUBLIC
    bb_core
)