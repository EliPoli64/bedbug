cmake_minimum_required(VERSION 3.28)

project(bedbug VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Sql Test)
qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/sqlgen)
add_subdirectory(src/app)
add_subdirectory(tests)