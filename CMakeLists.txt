cmake_minimum_required(VERSION 3.25)

project(MazeGame
    VERSION 1.0 
    DESCRIPTION "Maze Game"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core library (MazeBuilder/PathFinder/MazeViewer)
add_library(MazeCore
    src/core/MazeBuilder.cpp
    src/core/PathFinder.cpp
    src/core/MazeViewer.cpp
)

target_include_directories(MazeCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# App
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        MazeCore
)
