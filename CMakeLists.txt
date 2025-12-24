cmake_minimum_required(VERSION 3.25)

project(MazeGame
    VERSION 1.0 
    DESCRIPTION "Maze Game"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(SOURCES)

add_executable(${PROJECT_NAME} ${SOURCES})
