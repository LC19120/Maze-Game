#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

class MazeViewer
{
public:
    static MazeViewer& getInstance();

    MazeViewer(const MazeViewer&) = delete;
    MazeViewer& operator=(const MazeViewer&) = delete;

    // 任意线程可调用：更新迷宫数据（窗口线程会在下一帧重建网格并渲染）
    void setMaze(const Maze& newMaze);

    // 主线程调用：创建窗口 + 事件循环 + 实时渲染（直到窗口关闭或 requestClose）
    void run();

    // 任意线程可调用：请求关闭窗口
    void requestClose();

    bool isOpen() const;

private:
    MazeViewer();
    ~MazeViewer();

    void initWindowAndGL_();
    void shutdownGL_();

    void processEvents_();
    void renderFrame_();

    void rebuildMeshIfDirty_(); // mazeDirty_ 时重建 VBO
    void rebuildMeshFromMaze_(const Maze& m);

private:
    // maze data
    mutable std::mutex mazeMutex_;
    Maze maze_{};
    bool mazeLoaded_{false};
    bool mazeDirty_{false};

    // window control
    std::atomic<bool> closeRequested_{false};

    // OpenGL/GLFW handles (opaque here; defined in .cpp)
    void* window_{nullptr}; // actually GLFWwindow*
    unsigned int program_{0};
    unsigned int vao_{0};
    unsigned int vbo_{0};
    int fbW_{900};
    int fbH_{900};
    int vertexCount_{0};
};