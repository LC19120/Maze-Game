#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"
#include "core/MazeBuilder.hpp"
#include "core/PathFinder.hpp"

#include <stdexcept>
#include <algorithm>

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
    constexpr float VIS_PHASE = 0.70f; // 70% visited, 30% path

    const auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - anim.t0);
    if (elapsed < std::chrono::milliseconds(0)) elapsed = std::chrono::milliseconds(0);
    if (elapsed > TOTAL) elapsed = TOTAL;

    const auto visEnd = std::chrono::milliseconds((int)(TOTAL.count() * VIS_PHASE));
    const auto pathSpan = TOTAL - visEnd;

    size_t nVisited = 0;
    size_t nPath = 0;

    if (elapsed <= visEnd || pathSpan.count() <= 0)
    {
        const double t = (visEnd.count() > 0) ? (double)elapsed.count() / (double)visEnd.count() : 1.0;
        nVisited = (size_t)std::floor(t * (double)anim.visited.size());
        nPath = 0;
    }
    else
    {
        nVisited = anim.visited.size();
        const auto pe = elapsed - visEnd;
        const double t = (pathSpan.count() > 0) ? (double)pe.count() / (double)pathSpan.count() : 1.0;
        nPath = (size_t)std::floor(t * (double)anim.path.size());
    }

    nVisited = std::min(nVisited, anim.visited.size());
    nPath = std::min(nPath, anim.path.size());

    if (nVisited == anim.lastVisitedN && nPath == anim.lastPathN && elapsed != TOTAL)
        return;

    anim.lastVisitedN = nVisited;
    anim.lastPathN = nPath;

    const int H = (int)maze.grid.size();
    const int W = (int)maze.grid[0].size();

    // clear non-walls
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

    // paint visited (half alpha comes from maze_render.cpp via visitedVal mapping)
    for (size_t i = 0; i < nVisited; ++i)
    {
        const auto& p = anim.visited[i];
        if (maze.InBounds(p.x, p.y) && maze.grid[p.y][p.x] != 1)
            maze.grid[p.y][p.x] = anim.visitedVal;
    }

    // paint final shortest path (opaque)
    for (size_t i = 0; i < nPath; ++i)
    {
        const auto& p = anim.path[i];
        if (maze.InBounds(p.x, p.y) && maze.grid[p.y][p.x] != 1)
            maze.grid[p.y][p.x] = anim.pathVal;
    }

    // start/end markers
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

    // ensure endpoints are walkable
    if (maze.InBounds(sx, sy)) maze.grid[sy][sx] = 0;
    if (maze.InBounds(ex, ey)) maze.grid[ey][ex] = 0;

    std::vector<Point> path;
    std::vector<Point> visited;

    if (algoIndex == 1)
    {
        const int bc = std::clamp(uiBreakCount, 0, 9);
        auto result = WallBreaker::BreakWalls(maze, bc);
        path = std::get<0>(result);
        visited = std::get<1>(result);

        // BREAK button color mapping in maze_render.cpp:
        // path: 7, visited: 17
        anim.pathVal = 7;
        anim.visitedVal = 17;
    }
    else
    {
        auto result = PathFinder::pathFinder(maze);
        path = std::get<0>(result);
        visited = std::get<1>(result);

        // PATH button color mapping:
        // path: 5, visited: 15
        anim.pathVal = 5;
        anim.visitedVal = 15;
    }

    // start 3s animation playback
    anim.active = true;
    anim.t0 = std::chrono::steady_clock::now();
    anim.visited = std::move(visited);
    anim.path = std::move(path);
    anim.lastVisitedN = (size_t)-1;
    anim.lastPathN = (size_t)-1;

    // start from clean view
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


