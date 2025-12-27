#pragma once

#include "core/Common.hpp"
#include "core/DataStruct.hpp"
#include "core/PathFinder.hpp"

enum class UI
{
    None,
    Seed,
    BreakCount,
    StartX,
    StartY,
    EndX,
    EndY,
    UpdateEvery,
    DelayMs
};

class Viewer {
public:
    static Viewer& getInstance();
    void run();

    // +++ add: allow GLFW callback to update framebuffer size safely
    void onFramebufferResized(int width, int height);
    // --- add

private:
    Viewer();
    ~Viewer();

private:
    // window/gl
    void initWindowAndGL();
    void shutdownGL();
    void initUiCallbacks();
    void updateWindowTitle();

    void applyEdit();


    // render
    void drawMaze();
    void renderUi();
    void rebuildMeshFromMaze(const Maze& m);
    void rebuildMeshIfDirty();

    // work
    void buildMaze(int seed);
    void findPath(int sx, int sy, int ex, int ey, int algoIndex);


private:
    // -------- window / gl state --------
    void* window = nullptr;
    int fbW = 900;
    int fbH = 600;

    uint32_t program = 0;
    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t uiVao = 0;
    uint32_t uiVbo = 0;

    int vertexCount = 0;
    int uiVertexCount = 0;
    
    int meshBuiltFbW = 0;
    int meshBuiltFbH = 0;

    // -------- maze state shared with renderer --------
    Maze maze{};
    bool mazeLoaded = false;
    bool mazeDirty = false;

    Maze latestMaze{};
    bool hasMaze = false;

    // -------- UI state --------
    UI uiFocus = UI::None;
    std::string uiEdit;
    int uiSeed = 0;
    int uiBreakCount = 1;
    int uiStartX = 1, uiStartY = 1;
    int uiEndX = 1, uiEndY = 1;
    int uiUpdateEvery = 4;
    int uiDelayMs = 0;
    // algo: 0=PATH, 1=BREAK, 2=COUNT
    int uiAlgoIndex = 0;

};