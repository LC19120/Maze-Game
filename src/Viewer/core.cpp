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

void Viewer::run()
{
    initWindowAndGL();

    // 同步一次真实 framebuffer 尺寸（Retina 下窗口尺寸 != framebuffer 尺寸）
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

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (maze.grid[y][x] != 1) maze.grid[y][x] = 0;

    std::vector<Point> path;

    if (algoIndex == 1)
    {
        // BREAK: use uiBreakCount
        const int bc = std::clamp(uiBreakCount, 0, 99);
        auto result = WallBreaker::BreakWalls(maze, bc);
        path = std::get<0>(result);
    }
    else if (algoIndex == 2)
    {
        // COUNT: PathCounter
        auto result = PathCounter::CountPaths(maze, maze.start, maze.end);
        auto& allPaths = std::get<0>(result).first;

        // 先只渲染第一条路径（避免把所有路径都画满）
        if (!allPaths.empty())
            path = allPaths.front();
    }
    else
    {
        // PATH
        auto result = PathFinder::pathFinder(maze);
        path = std::get<0>(result);
    }

    // path paint (still using 5 = blue path)
    for (const auto& p : path)
    {
        if (maze.InBounds(p.x, p.y) && maze.grid[p.y][p.x] != 1)
            maze.grid[p.y][p.x] = 5;
    }

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


