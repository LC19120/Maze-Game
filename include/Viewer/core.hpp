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
    void onFramebufferResized(int width, int height);

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
    void tickPathAnim_();
    void passPath(uint32_t x, uint32_t y);

    struct PathAnim
    {
        bool active = false;
        std::chrono::steady_clock::time_point t0{};

        // mode 0: PATH/BREAK (visited+path), mode 1: COUNT overlay
        int mode = 0;

        // PATH/BREAK playback data
        std::vector<Point> visited;
        std::vector<Point> path;
        int visitedVal = 15;
        int pathVal    = 5;

        bool hasOrigWall = false;
        std::vector<uint8_t> origWall;

        size_t lastVisitedN = (size_t)-1;
        size_t lastPathN    = (size_t)-1;

        // COUNT playback data
        std::vector<std::vector<Point>> allPaths;
        int totalPaths = 0;

        std::vector<size_t> lastLenPerPath;

        std::vector<int32_t> passCount;
    } anim;

    bool alphaOverrideActive = false;
    std::vector<float> cellAlphaOverride;

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
    int uiAlgoIndex = 0;

    // results shown in the "path info" box
    int lastPathLen  = 0;   // PATH (A*)
    int lastBreakLen = 0;   // BREAK
    int lastCountWays = 0;  // COUNT (already existed)
    int lastPassLen  = 0;   // PASS
    
    std::vector<uint8_t> baseWall; // 1 = wall, 0 = empty

};