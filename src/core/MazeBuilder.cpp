#include "core/MazeBuilder.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <array>

Maze MazeBuilder::Build(int seed)
{
    const int32_t SIZE = 41;

    Maze maze;
    maze.seed = seed;
    maze.grid.assign(SIZE, std::vector<int32_t>(SIZE, 1));

    std::mt19937 rng(seed);

    auto inBounds = [&](int x, int y) {
        return x > 0 && y > 0 && x < SIZE - 1 && y < SIZE - 1;
    };

    auto carve = [&](int x, int y) {
        maze.grid[y][x] = 0;
    };

    // start
    carve(1, 1);

    // step=2 grid (cells at odd coords)
    const int dx[4] = { 2, -2, 0, 0 };
    const int dy[4] = { 0, 0, 2, -2 };

    std::vector<Point> st;
    st.push_back({ 1, 1});

    // Perfect maze (unique path)
    while (!st.empty())
    {
        Point cur = st.back();

        std::array<int, 4> dirs = { 0, 1, 2, 3 };
        std::shuffle(dirs.begin(), dirs.end(), rng);

        bool moved = false;
        for (int dir : dirs)
        {
            const int nx = cur.x + dx[dir];
            const int ny = cur.y + dy[dir];

            if (!inBounds(nx, ny)) continue;
            if (maze.grid[ny][nx] != 1) continue; // only carve into unvisited

            carve(cur.x + dx[dir] / 2, cur.y + dy[dir] / 2);
            carve(nx, ny);

            st.push_back({ (int32_t)nx, (int32_t)ny});
            moved = true;
            break; // fewer branches
        }

        if (!moved)
            st.pop_back();
    }

    // +++ add: braid ~10 walls to create ~multiple routes (cycles)
    // This increases the number of distinct paths from (1,1) to (SIZE-2,SIZE-2).
    constexpr int EXTRA_LOOPS = 10;

    struct WallCell { int x; int y; };
    std::vector<WallCell> candidates;
    candidates.reserve((size_t)SIZE * (size_t)SIZE / 4);

    for (int y = 1; y < SIZE - 1; ++y)
    {
        for (int x = 1; x < SIZE - 1; ++x)
        {
            if (maze.grid[y][x] != 1) continue;

            // wall between two corridors (either horizontal or vertical)
            const bool horiz = (maze.grid[y][x - 1] == 0 && maze.grid[y][x + 1] == 0);
            const bool vert  = (maze.grid[y - 1][x] == 0 && maze.grid[y + 1][x] == 0);
            if (horiz || vert)
                candidates.push_back({ x, y });
        }
    }

    std::shuffle(candidates.begin(), candidates.end(), rng);

    int opened = 0;
    for (const auto& w : candidates)
    {
        if (opened >= EXTRA_LOOPS) break;
        if (maze.grid[w.y][w.x] == 1)
        {
            carve(w.x, w.y);
            ++opened;
        }
    }

    return maze;
}