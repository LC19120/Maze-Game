#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"
#include "core/MazeBuilder.hpp"
#include "core/PathFinder.hpp"

#include <stdexcept>
#include <algorithm>
#include <cmath> // +++ add for std::floor

Viewer& Viewer::getInstance()
{
    static Viewer inst;
    return inst;
}

Viewer::Viewer() = default;

Viewer::~Viewer()
{
    // allow safe cleanup even if run() already called shutdownGL()
    try { shutdownGL(); } catch (...) {}
}

void Viewer::tickPathAnim_()
{
    if (!anim.active) return;
    if (!mazeLoaded) { anim.active = false; return; }
    if (maze.grid.empty() || maze.grid[0].empty()) { anim.active = false; return; }

    constexpr auto TOTAL = std::chrono::milliseconds(3000);

    const auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - anim.t0);
    if (elapsed < std::chrono::milliseconds(0)) elapsed = std::chrono::milliseconds(0);
    if (elapsed > TOTAL) elapsed = TOTAL;

    const int H = (int)maze.grid.size();
    const int W = (int)maze.grid[0].size();

    // ---- MODE 1: COUNT overlay animation (3s for all paths)
    if (anim.mode == 1)
    {
        if (anim.totalPaths <= 0 || anim.allPaths.empty())
        {
            anim.active = false;
            alphaOverrideActive = false;
            cellAlphaOverride.clear();
            return;
        }

        const double t = (TOTAL.count() > 0) ? (double)elapsed.count() / (double)TOTAL.count() : 1.0;
        size_t k = (size_t)std::floor(t * (double)anim.totalPaths);
        if (k > (size_t)anim.totalPaths) k = (size_t)anim.totalPaths;

        // first tick init buffers (size W*H)
        const size_t N = (size_t)W * (size_t)H;
        if (anim.passCount.size() != N) anim.passCount.assign(N, 0);
        if (cellAlphaOverride.size() != N) cellAlphaOverride.assign(N, 0.0f);
        alphaOverrideActive = true;

        // apply newly revealed paths incrementally
        if (k > anim.lastK)
        {
            for (size_t i = anim.lastK; i < k; ++i)
            {
                if (i >= anim.allPaths.size()) break;
                for (const auto& p : anim.allPaths[i])
                {
                    if (!maze.InBounds(p.x, p.y)) continue;
                    if (maze.grid[p.y][p.x] == 1) continue; // don't paint walls

                    const size_t idx = (size_t)p.y * (size_t)W + (size_t)p.x;
                    int32_t cnt = ++anim.passCount[idx];

                    // each path contributes 1/totalPaths, overlap accumulates
                    float a = (float)cnt / (float)anim.totalPaths;
                    if (a > 1.0f) a = 1.0f;

                    cellAlphaOverride[idx] = a;
                    maze.grid[p.y][p.x] = 6; // COUNT button color
                }
            }

            anim.lastK = k;
            mazeDirty = true;
        }

        // keep endpoints visible
        if (maze.InBounds(maze.start.x, maze.start.y)) maze.grid[maze.start.y][maze.start.x] = 27;
        if (maze.InBounds(maze.end.x, maze.end.y))     maze.grid[maze.end.y][maze.end.x]     = 27;

        if (elapsed == TOTAL)
            anim.active = false;

        return;
    }

    // ---- MODE 0: PATH/BREAK (existing visited->path split)
    constexpr float VIS_PHASE = 0.70f; // 70% visited, 30% path
    const auto visEnd = std::chrono::milliseconds((int)(TOTAL.count() * VIS_PHASE));
    const auto pathSpan = TOTAL - visEnd;

    size_t nVisited = 0;
    size_t nPath = 0;

    if (elapsed <= visEnd || pathSpan.count() <= 0)
    {
        const double tt = (visEnd.count() > 0) ? (double)elapsed.count() / (double)visEnd.count() : 1.0;
        nVisited = (size_t)std::floor(tt * (double)anim.visited.size());
        nPath = 0;
    }
    else
    {
        nVisited = anim.visited.size();
        const auto pe = elapsed - visEnd;
        const double tt = (pathSpan.count() > 0) ? (double)pe.count() / (double)pathSpan.count() : 1.0;
        nPath = (size_t)std::floor(tt * (double)anim.path.size());
    }

    nVisited = std::min(nVisited, anim.visited.size());
    nPath = std::min(nPath, anim.path.size());

    if (nVisited == anim.lastVisitedN && nPath == anim.lastPathN && elapsed != TOTAL)
        return;

    anim.lastVisitedN = nVisited;
    anim.lastPathN = nPath;

    // clear non-walls
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

    // paint visited
    for (size_t i = 0; i < nVisited; ++i)
    {
        const auto& p = anim.visited[i];
        if (maze.InBounds(p.x, p.y) && maze.grid[p.y][p.x] != 1)
            maze.grid[p.y][p.x] = anim.visitedVal;
    }

    // paint path
    for (size_t i = 0; i < nPath; ++i)
    {
        const auto& p = anim.path[i];
        if (maze.InBounds(p.x, p.y) && maze.grid[p.y][p.x] != 1)
            maze.grid[p.y][p.x] = anim.pathVal;
    }

    if (maze.InBounds(maze.start.x, maze.start.y)) maze.grid[maze.start.y][maze.start.x] = 27;
    if (maze.InBounds(maze.end.x, maze.end.y))     maze.grid[maze.end.y][maze.end.x]     = 27;

    mazeDirty = true;

    if (elapsed == TOTAL)
        anim.active = false;
}

void Viewer::run()
{
    initWindowAndGL();

    if (window)
    {
        int w = 1, h = 1;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &w, &h);
        fbW = std::max(1, w);
        fbH = std::max(1, h);
        glViewport(0, 0, fbW, fbH);
    }

    // 初始生成一个迷宫，避免空白
    buildMaze(uiSeed);

    auto* win = static_cast<GLFWwindow*>(window);
    while (win && !glfwWindowShouldClose(win))
    {
        tickPathAnim_(); // +++ add

        // clear
        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // render
        drawMaze();
        renderUi();

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    shutdownGL();
}

void Viewer::updateWindowTitle()
{
    if (!window) return;

    std::string title = "Maze Viewer  |  seed=" + std::to_string(uiSeed);
    glfwSetWindowTitle(static_cast<GLFWwindow*>(window), title.c_str());
}

void Viewer::buildMaze(int seed)
{
    uiSeed = seed;

    Maze m = MazeBuilder::Build(seed);

    // 补齐 Maze 的 width/height/start/end（PathFinder.cpp 里依赖这些字段）
    m.height = (int)m.grid.size();
    m.width  = (m.height > 0) ? (int)m.grid[0].size() : 0;

    m.start = {1, 1, "start"};
    m.end   = {std::max(1, m.width - 2), std::max(1, m.height - 2), "end"};

    maze = std::move(m);
    mazeLoaded = true;
    mazeDirty = true;

    // 同步 UI 的起终点
    uiStartX = maze.start.x; uiStartY = maze.start.y;
    uiEndX   = maze.end.x;   uiEndY   = maze.end.y;

    updateWindowTitle();
}

void Viewer::findPath(int sx, int sy, int ex, int ey, int algoIndex)
{
    if (!mazeLoaded) return;

    uiAlgoIndex = algoIndex;

    const int H = (int)maze.grid.size();
    const int W = (H > 0) ? (int)maze.grid[0].size() : 0;
    if (W <= 0 || H <= 0) return;

    sx = std::clamp(sx, 0, W - 1);
    sy = std::clamp(sy, 0, H - 1);
    ex = std::clamp(ex, 0, W - 1);
    ey = std::clamp(ey, 0, H - 1);

    maze.start = {sx, sy, "start"};
    maze.end   = {ex, ey, "end"};
    maze.width = W;
    maze.height = H;

    if (maze.InBounds(sx, sy)) maze.grid[sy][sx] = 0;
    if (maze.InBounds(ex, ey)) maze.grid[ey][ex] = 0;

    // reset alpha override unless COUNT starts it
    alphaOverrideActive = false;
    cellAlphaOverride.clear();

    // COUNT: animate all paths as overlay
    if (algoIndex == 2)
    {
        auto result = PathCounter::CountPaths(maze, maze.start, maze.end);
        auto allPaths = std::move(std::get<0>(result).first);
        const int32_t ways = std::get<1>(result);

        // clear non-walls once
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

        if (maze.InBounds(maze.start.x, maze.start.y)) maze.grid[maze.start.y][maze.start.x] = 27;
        if (maze.InBounds(maze.end.x, maze.end.y))     maze.grid[maze.end.y][maze.end.x]     = 27;

        anim.active = true;
        anim.mode = 1;
        anim.t0 = std::chrono::steady_clock::now();
        anim.allPaths = std::move(allPaths);
        anim.totalPaths = std::max<int>(0, ways);
        anim.lastK = 0;
        anim.passCount.clear();

        mazeDirty = true;
        updateWindowTitle();
        return;
    }

    // PATH / BREAK: existing animation
    std::vector<Point> path;
    std::vector<Point> visited;

    if (algoIndex == 1)
    {
        const int bc = std::clamp(uiBreakCount, 0, 9);
        auto result = WallBreaker::BreakWalls(maze, bc);
        path = std::get<0>(result);
        visited = std::get<1>(result);
        anim.pathVal = 7;
        anim.visitedVal = 17;
    }
    else
    {
        auto result = PathFinder::pathFinder(maze);
        path = std::get<0>(result);
        visited = std::get<1>(result);
        anim.pathVal = 5;
        anim.visitedVal = 15;
    }

    anim.active = true;
    anim.mode = 0;
    anim.t0 = std::chrono::steady_clock::now();
    anim.visited = std::move(visited);
    anim.path = std::move(path);
    anim.lastVisitedN = (size_t)-1;
    anim.lastPathN = (size_t)-1;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

    if (maze.InBounds(maze.start.x, maze.start.y)) maze.grid[maze.start.y][maze.start.x] = 27;
    if (maze.InBounds(maze.end.x, maze.end.y))     maze.grid[maze.end.y][maze.end.x]     = 27;

    mazeDirty = true;
    updateWindowTitle();
}

void Viewer::onFramebufferResized(int width, int height)
{
    fbW = std::max(1, width);
    fbH = std::max(1, height);
    // 不需要在这里 rebuild mesh；drawMaze() 每帧都会按 fbW/fbH 设 viewport
}


