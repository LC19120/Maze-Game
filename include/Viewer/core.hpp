#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "Thread/ThreadPool.hpp"

class MazeViewer
{
public:
    static MazeViewer& getInstance();

    MazeViewer(const MazeViewer&) = delete;
    MazeViewer& operator=(const MazeViewer&) = delete;

    void setMaze(const Maze& newMaze);
    void run();
    void requestClose();
    bool isOpen() const;

private:
    MazeViewer();
    ~MazeViewer();

    void initWindowAndGL_();
    void shutdownGL_();

    void processEvents_();
    void renderFrame_();

    void rebuildMeshIfDirty_();
    void rebuildMeshFromMaze_(const Maze& m);

    // UI (custom, no ImGui)
    void initUiCallbacks_();
    void renderUi_();
    void updateWindowTitle_();

    // UI actions (runs tasks in thread pool)
    void requestBuild_(MazeType type, int32_t seed);
    void requestFindPath_(int32_t sx, int32_t sy, int32_t ex, int32_t ey);
    void cancelWork_();

    // +++ add
    void applyEdit_();
    // --- add

private:
    // maze data
    mutable std::mutex mazeMutex_;
    Maze maze_{};
    bool mazeLoaded_{false};
    bool mazeDirty_{false};

    // also keep latest maze snapshot for path-finding
    mutable std::mutex latestMazeMutex_;
    Maze latestMaze_{};
    bool hasMaze_{false};

    // work control
    ThreadPool pool_;
    std::atomic<uint64_t> latestToken_{0};
    mutable std::mutex cancelMutex_;
    std::shared_ptr<std::atomic<bool>> cancelFlag_{std::make_shared<std::atomic<bool>>(false)};

    // UI state
    int uiSeed_{0};
    int uiTypeIndex_{0}; // 0..3 => Small/Medium/Large/Ultra
    int uiStartX_{1}, uiStartY_{1};
    int uiEndX_{1}, uiEndY_{1};
    int uiUpdateEvery_{5};
    int uiDelayMs_{1};

    enum class UiField { None, Seed, StartX, StartY, EndX, EndY, UpdateEvery, DelayMs };
    UiField uiFocus_{UiField::None};
    std::string uiEdit_{};

    mutable std::mutex uiMsgMutex_;
    std::string uiLastMsg_;

    // window control
    std::atomic<bool> closeRequested_{false};

    // OpenGL/GLFW handles
    void* window_{nullptr}; // actually GLFWwindow*
    unsigned int program_{0};

    unsigned int vao_{0};
    unsigned int vbo_{0};
    int vertexCount_{0};

    // UI geometry (separate buffer)
    unsigned int uiVao_{0};
    unsigned int uiVbo_{0};
    int uiVertexCount_{0};

    int fbW_{900};
    int fbH_{900};

    // +++ add: split maze draw out of core.cpp
    void drawMaze_();
    // --- add
};