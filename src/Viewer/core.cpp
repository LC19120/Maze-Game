#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"
#include "core/MazeBuilder.hpp"
#include "core/PathFinder.hpp"

#include <stdexcept>
#include <algorithm>
#include <cmath>

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
    const size_t N = (size_t)W * (size_t)H;

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

        const double t = (TOTAL.count() > 0)
            ? (double)elapsed.count() / (double)TOTAL.count()
            : 1.0;

        if (anim.passCount.size() != N) anim.passCount.assign(N, 0);
        if (cellAlphaOverride.size() != N) cellAlphaOverride.assign(N, 0.0f);
        alphaOverrideActive = true;

        if (anim.lastLenPerPath.size() != anim.allPaths.size())
            anim.lastLenPerPath.assign(anim.allPaths.size(), 0);

        bool anyChanged = false;

        // 让所有路径“从起点同时开始增长”
        for (size_t i = 0; i < anim.allPaths.size(); ++i)
        {
            const auto& one = anim.allPaths[i];
            if (one.empty()) continue;

            const size_t targetLen = std::min(one.size(), (size_t)std::floor(t * (double)one.size()));
            size_t& lastLen = anim.lastLenPerPath[i];

            if (targetLen <= lastLen) continue;

            for (size_t j = lastLen; j < targetLen; ++j)
            {
                const auto& p = one[j];
                if (!maze.InBounds(p.x, p.y)) continue;
                if (maze.grid[p.y][p.x] == 1) continue;

                const size_t idx = (size_t)p.y * (size_t)W + (size_t)p.x;
                const int32_t cnt = ++anim.passCount[idx];

                float a = (float)cnt / (float)anim.totalPaths;
                if (a > 1.0f) a = 1.0f;

                cellAlphaOverride[idx] = a;
                maze.grid[p.y][p.x] = 6; // COUNT color (violet)
            }

            lastLen = targetLen;
            anyChanged = true;
        }

        if (anyChanged)
            mazeDirty = true;

        if (elapsed == TOTAL)
            anim.active = false;

        return;
    }

    // ---- MODE 0: PATH/BREAK (visited->path split)
    constexpr float VIS_PHASE = 0.70f;
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
        if (!maze.InBounds(p.x, p.y)) continue;
        if (maze.grid[p.y][p.x] == 1) continue;

        // 允许 start/end 也显示 visited 颜色
        // if ((p.x == maze.start.x && p.y == maze.start.y) ||
        //     (p.x == maze.end.x   && p.y == maze.end.y))
        //     continue;

        maze.grid[p.y][p.x] = anim.visitedVal;
    }

    // paint path
    for (size_t i = 0; i < nPath; ++i)
    {
        const auto& p = anim.path[i];
        if (!maze.InBounds(p.x, p.y)) continue;

        const size_t idx = (size_t)p.y * (size_t)W + (size_t)p.x;

        
        if (maze.grid[p.y][p.x] == 1)
        {
            if (anim.pathVal == 7 && anim.hasOrigWall && anim.origWall.size() == N && anim.origWall[idx] == 1)
            {
                maze.grid[p.y][p.x] = 18; // special: broken-wall-on-path
            }
            continue;
        }

        // BREAK: if this path cell was a wall in the ORIGINAL maze => render as "broken" marker
        if (anim.pathVal == 7 && anim.hasOrigWall && anim.origWall.size() == N)
        {
            if (anim.origWall[idx] == 1)
            {
                maze.grid[p.y][p.x] = 18;
                continue;
            }
        }

        maze.grid[p.y][p.x] = anim.pathVal;
    }

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

    m.height = (int)m.grid.size();
    m.width  = (m.height > 0) ? (int)m.grid[0].size() : 0;

    m.start = {1, 1};
    m.end   = {std::max(1, m.width - 2), std::max(1, m.height - 2)};

    maze = std::move(m);
    mazeLoaded = true;
    mazeDirty = true;

    // 同步 UI 的起终点
    uiStartX = maze.start.x; uiStartY = maze.start.y;
    uiEndX   = maze.end.x;   uiEndY   = maze.end.y;

    // +++ add: snapshot base walls
    const int H = (int)maze.grid.size();
    const int W = (H > 0) ? (int)maze.grid[0].size() : 0;
    baseWall.assign((size_t)W * (size_t)H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            baseWall[(size_t)y * (size_t)W + (size_t)x] = (maze.grid[y][x] == 1) ? 1 : 0;
    // --- add

    updateWindowTitle();
}

void Viewer::findPath(int sx, int sy, int ex, int ey, int algoIndex)
{
    if (!mazeLoaded) return;

    uiAlgoIndex = algoIndex;

    const int H = (int)maze.grid.size();
    const int W = (H > 0) ? (int)maze.grid[0].size() : 0;
    if (W <= 0 || H <= 0) return;

    const size_t N = (size_t)W * (size_t)H;

    // +++ add: restore maze from baseWall (prevents previous animations from altering topology)
    if (baseWall.size() == N)
    {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                maze.grid[y][x] = baseWall[(size_t)y * (size_t)W + (size_t)x] ? 1 : 0;
    }
    else
    {
        // fallback
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;
    }
    // --- add

    sx = std::clamp(sx, 0, W - 1);
    sy = std::clamp(sy, 0, H - 1);
    ex = std::clamp(ex, 0, W - 1);
    ey = std::clamp(ey, 0, H - 1);

    maze.start = {sx, sy};
    maze.end   = {ex, ey};
    maze.width = W;
    maze.height = H;

    // keep endpoints walkable (and keep baseWall consistent too)
    if (maze.InBounds(sx, sy)) maze.grid[sy][sx] = 0;
    if (maze.InBounds(ex, ey)) maze.grid[ey][ex] = 0;

    // +++ add: reflect endpoint carve into baseWall so future restores match
    if (baseWall.size() == N)
    {
        baseWall[(size_t)sy * (size_t)W + (size_t)sx] = 0;
        baseWall[(size_t)ey * (size_t)W + (size_t)ex] = 0;
    }
    // --- add

    alphaOverrideActive = false;
    cellAlphaOverride.clear();

    // COUNT
    if (algoIndex == 2)
    {
        auto result = PathCounter::CountPaths(maze, maze.start, maze.end);
        auto allPaths = std::move(std::get<0>(result).first);
        const int32_t ways = std::get<1>(result);

        lastCountWays = (int)std::max<int32_t>(0, ways);

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

        anim.active = true;
        anim.mode = 1;
        anim.t0 = std::chrono::steady_clock::now();
        anim.allPaths = std::move(allPaths);
        anim.totalPaths = std::max<int>(0, ways);

        // +++ add: init per-path progress (start together)
        anim.lastLenPerPath.assign(anim.allPaths.size(), 0);
        // --- add

        anim.passCount.clear();

        anim.hasOrigWall = false;
        anim.origWall.clear();

        mazeDirty = true;
        updateWindowTitle();
        return;
    }

    // PATH / BREAK
    std::vector<Point> path;
    std::vector<Point> visited;

    anim.hasOrigWall = false;
    anim.origWall.clear();

    if (algoIndex == 1)
    {
        // snapshot original walls BEFORE running breaker
        anim.hasOrigWall = true;
        anim.origWall.resize((size_t)W * (size_t)H, 0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                anim.origWall[(size_t)y * (size_t)W + (size_t)x] = (maze.grid[y][x] == 1) ? 1 : 0;

        const int bc = std::clamp(uiBreakCount, 0, 9);
        auto result = WallBreaker::BreakWalls(maze, bc);
        path = std::get<0>(result);
        visited = std::get<1>(result);

        lastBreakLen = (int)path.size();   // +++ add

        anim.pathVal = 7;
        anim.visitedVal = 17;
    }
    else
    {
        auto result = PathFinder::pathFinder(maze);
        path = std::get<0>(result);
        visited = std::get<1>(result);

        lastPathLen = (int)path.size();    // +++ add

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

    mazeDirty = true;
    updateWindowTitle();
}

void Viewer::onFramebufferResized(int width, int height)
{
    fbW = std::max(1, width);
    fbH = std::max(1, height);
    // 不需要在这里 rebuild mesh；drawMaze() 每帧都会按 fbW/fbH 设 viewport
}

// +++ add: PathPasser button action (force start/end = 1,1 -> size-2,size-2; go through (x,y))
void Viewer::passPath(uint32_t x, uint32_t y)
{
    if (!mazeLoaded) return;

    const int H = (int)maze.grid.size();
    const int W = (H > 0) ? (int)maze.grid[0].size() : 0;
    if (W <= 0 || H <= 0) return;

    const size_t N = (size_t)W * (size_t)H;

    // restore topology from baseWall (prevents previous animations from changing the maze)
    if (baseWall.size() == N)
    {
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
                maze.grid[yy][xx] = baseWall[(size_t)yy * (size_t)W + (size_t)xx] ? 1 : 0;
    }

    const int sx = 1;
    const int sy = 1;
    const int ex = std::max(1, W - 2);
    const int ey = std::max(1, H - 2);

    maze.start = { sx, sy };
    maze.end   = { ex, ey };
    maze.width = W;
    maze.height = H;

    // keep endpoints walkable
    if (maze.InBounds(sx, sy)) maze.grid[sy][sx] = 0;
    if (maze.InBounds(ex, ey)) maze.grid[ey][ex] = 0;

    // keep baseWall consistent with carved endpoints
    if (baseWall.size() == N)
    {
        baseWall[(size_t)sy * (size_t)W + (size_t)sx] = 0;
        baseWall[(size_t)ey * (size_t)W + (size_t)ex] = 0;
    }

    // clamp mid
    const int mx = std::clamp<int>((int)x, 0, W - 1);
    const int my = std::clamp<int>((int)y, 0, H - 1);

    alphaOverrideActive = false;
    cellAlphaOverride.clear();

    auto result = PathPasser::PassPath(maze, (uint32_t)mx, (uint32_t)my);

    auto path     = std::move(std::get<0>(result));
    auto visited1 = std::move(std::get<1>(result));
    auto visited2 = std::move(std::get<2>(result));

    visited1.insert(visited1.end(), visited2.begin(), visited2.end());

    anim.hasOrigWall = false;
    anim.origWall.clear();

    anim.active = true;
    anim.mode = 0;
    anim.t0 = std::chrono::steady_clock::now();
    anim.visited = std::move(visited1);
    anim.path = std::move(path);

    lastPassLen = (int)anim.path.size();

    // PASS color tiles (match PASS button)
    anim.visitedVal = 19;
    anim.pathVal    = 8;

    anim.lastVisitedN = (size_t)-1;
    anim.lastPathN = (size_t)-1;

    // clear for animation base using baseWall (prevents wall corruption)
    if (baseWall.size() == N)
    {
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
                maze.grid[yy][xx] = baseWall[(size_t)yy * (size_t)W + (size_t)xx] ? 1 : 0;

        maze.grid[sy][sx] = 0;
        maze.grid[ey][ex] = 0;
    }
    else
    {
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
                if (maze.grid[yy][xx] != 1) maze.grid[yy][xx] = 0;
    }

    mazeDirty = true;
    updateWindowTitle();
}
