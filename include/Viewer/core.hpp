#pragma once

#include "core/Common.hpp"
#include "core/DataStruct.hpp"
#include "core/MazeBuilder.hpp"

#include <array>
#include <string>

// UI focus fields (used by input.cpp)
enum class UiField
{
    None,
    Seed,
    StartX,
    StartY,
    EndX,
    EndY,
    UpdateEvery,
    DelayMs
};

class MazeViewer final
{
public:
    static MazeViewer& getInstance();
    void run();

private:
    MazeViewer();
    ~MazeViewer() = default;

    MazeViewer(const MazeViewer&) = delete;
    MazeViewer& operator=(const MazeViewer&) = delete;

private:
    // window/gl
    void initWindowAndGL_();
    void shutdownGL_();
    void initUiCallbacks_();
    void updateWindowTitle_();

    // render
    void drawMaze_();
    void renderUi_();

    void rebuildMeshFromMaze_(const Maze& m);
    void rebuildMeshIfDirty_();

    // work
    void requestBuild_(int seed);
    void requestFindPath_(int sx, int sy, int ex, int ey, int algoIndex);
    void cancelWork_();

    // NOTE: declared in input.cpp
    void applyEdit_();

private:
    // -------- window / gl state --------
    void* window_{nullptr};
    int fbW_{900};
    int fbH_{600};

    uint32_t program_{0};
    uint32_t vao_{0};
    uint32_t vbo_{0};
    uint32_t uiVao_{0};
    uint32_t uiVbo_{0};

    int vertexCount_{0};
    int uiVertexCount_{0};

    // +++ track the fb size used to build the maze mesh (so resize can trigger rebuild)
    int meshBuiltFbW_{0};
    int meshBuiltFbH_{0};
    // --- track

    // -------- maze state shared with renderer --------
    std::mutex mazeMutex_;
    Maze maze_{};
    bool mazeLoaded_{false};
    bool mazeDirty_{false};

    std::mutex latestMazeMutex_;
    Maze latestMaze_{};
    bool hasMaze_{false};

    // -------- UI state --------
    UiField uiFocus_{UiField::None};
    std::string uiEdit_;

    int uiSeed_{0};
    int uiStartX_{1}, uiStartY_{1};
    int uiEndX_{1}, uiEndY_{1};
    int uiUpdateEvery_{4};
    int uiDelayMs_{0};
    int uiAlgoIndex_{0};

    std::mutex uiMsgMutex_;
    std::string uiLastMsg_;

    // -------- algo stats (ui.cpp reads these) --------
    std::mutex algoLenMutex_;
    std::array<int, 6> algoPathLen_{};
    std::array<int, 6> algoVisited_{};
    std::array<int, 6> algoFoundAt_{};

    // -------- cancellation --------
    std::atomic<bool> cancel_{false};

    // (ThreadPool removed for UI stage)
};