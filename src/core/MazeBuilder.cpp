#include "core/MazeBuilder.hpp"
#include <vector>
#include <random>
#include <algorithm>
Maze MazeBuilder::Build(int seed)
{
    const int32_t SIZE = 71;

    Maze maze;
    maze.seed = seed;
    maze.info = "Multi-path random maze (branch-limited, no loops)";

    // 1 = wall, 0 = path
    maze.grid.resize(SIZE, std::vector<int32_t>(SIZE, 1));

    std::mt19937 rng(seed);
    std::vector<Point> stack;

    int branchCount = 0;

    auto inBounds = [&](int x, int y) {
        return x > 0 && y > 0 && x < SIZE - 1 && y < SIZE - 1;
    };

    auto carve = [&](int x, int y) {
        maze.grid[y][x] = 0;
    };

    // 起点
    Point start{1, 1, "start"};
    carve(1, 1);
    stack.push_back(start);

    const int dx[4] = { 2, -2, 0, 0 };
    const int dy[4] = { 0, 0, 2, -2 };

    while (!stack.empty())
    {
        Point current = stack.back();
        stack.pop_back();

        std::vector<int> dirs = { 0, 1, 2, 3 };
        std::shuffle(dirs.begin(), dirs.end(), rng);

        for (int dir : dirs)
        {
            // 支路数量达到上限后，不再继续生成新分支
            if (branchCount >= MAX_BRANCHES)
                break;

            int nx = current.x + dx[dir];
            int ny = current.y + dy[dir];

            // 仅允许向“未访问区域”挖掘，严格避免成环
            if (inBounds(nx, ny) && maze.grid[ny][nx] == 1)
            {
                // 打通中间墙
                carve(current.x + dx[dir] / 2,
                      current.y + dy[dir] / 2);

                // 打通目标格
                carve(nx, ny);

                stack.push_back({ (int32_t)nx, (int32_t)ny, "" });

                branchCount++;   // ⭐ 记录一条支路
            }
        }
    }

    return maze;
}
