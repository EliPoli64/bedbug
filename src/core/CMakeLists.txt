add_library(bb_core STATIC
    Version.cpp
)

target_include_directories(bb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bb_core PUBLIC
    Qt6::Core
)