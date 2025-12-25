cmake_minimum_required(VERSION 3.25)

project(MazeGame
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------
# Third-party (Mac)
# -----------------------------
set(THIRDPARTY_MAC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdPart/Mac")
set(THIRDPARTY_MAC_INC "${THIRDPARTY_MAC_DIR}/include")
set(THIRDPARTY_MAC_LIB "${THIRDPARTY_MAC_DIR}/lib")

# glad (build from source)
add_library(glad STATIC
    "${THIRDPARTY_MAC_DIR}/src/glad.c"
)
target_include_directories(glad PUBLIC "${THIRDPARTY_MAC_INC}")

# glfw (prebuilt static lib) - adjust name if your file differs
add_library(glfw STATIC IMPORTED GLOBAL)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${THIRDPARTY_MAC_LIB}/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRDPARTY_MAC_INC}"
)

# macOS frameworks required by glfw/opengl
find_library(FW_COCOA Cocoa REQUIRED)
find_library(FW_IOKIT IOKit REQUIRED)
find_library(FW_COREVIDEO CoreVideo REQUIRED)
find_library(FW_OPENGL OpenGL REQUIRED)

# -----------------------------
# Core library
# -----------------------------
add_library(MazeCore
    src/core/MazeBuilder.cpp
    src/core/PathFinder.cpp
    src/core/MazeViewer.cpp
    src/Thread/ThreadPool.cpp
    src/app.cpp
    src/Exploer/Exploer.cpp
)

target_include_directories(MazeCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${THIRDPARTY_MAC_INC}"
)

target_link_libraries(MazeCore
    PUBLIC
        glad
        glfw
        "${FW_COCOA}"
        "${FW_IOKIT}"
        "${FW_COREVIDEO}"
        "${FW_OPENGL}"
)

# -----------------------------
# App
# -----------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        MazeCore
)
