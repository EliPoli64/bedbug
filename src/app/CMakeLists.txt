qt_add_executable(bedbug
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(bedbug PRIVATE
    Qt6::Widgets
    bb_core
)