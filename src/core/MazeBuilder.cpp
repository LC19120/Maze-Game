#include "core/MazeBuilder.hpp"
#include <vector>
#include <random>
#include <algorithm>
Maze MazeBuilder::Build(int seed)
{
    const int32_t SIZE = 41;

    Maze maze;
    maze.seed = seed;
    maze.info = "Multi-path random maze";

    maze.grid.resize(SIZE, std::vector<int32_t>(SIZE, 1));

    std::mt19937 rng(seed);
    std::vector<Point> stack;

    auto inBounds = [&](int x, int y) {
        return x > 0 && y > 0 && x < SIZE - 1 && y < SIZE - 1;
    };

    auto carve = [&](int x, int y) {
        maze.grid[y][x] = 0;
    };

    Point start{1, 1, "start"};
    carve(1, 1);
    stack.push_back(start);

    const int dx[4] = {2, -2, 0, 0};
    const int dy[4] = {0, 0, 2, -2};

    while (!stack.empty())
    {
        Point current = stack.back();
        stack.pop_back();

        std::vector<int> dirs = {0, 1, 2, 3};
        std::shuffle(dirs.begin(), dirs.end(), rng);

        for (int dir : dirs)
        {
            int nx = current.x + dx[dir];
            int ny = current.y + dy[dir];

            if (inBounds(nx, ny) && maze.grid[ny][nx] == 1)
            {
                carve(current.x + dx[dir] / 2, current.y + dy[dir] / 2);
                carve(nx, ny);
                stack.push_back({(int32_t)nx, (int32_t)ny, ""});
            }
            else if (inBounds(nx, ny) && maze.grid[ny][nx] == 0)
            {
                if (rng() % 5 == 0)
                    carve(current.x + dx[dir] / 2, current.y + dy[dir] / 2);
            }
        }
    }

    return maze;
}