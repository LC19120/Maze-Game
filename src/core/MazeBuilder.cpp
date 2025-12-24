#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

Maze MazeBuilder::Build(
    MazeType type,
    int32_t seed,
    const std::function<void(const Maze&)>& onUpdate,
    std::atomic<bool>* cancel,
    uint32_t updateEvery,
    std::chrono::milliseconds delay)
{
    const auto s = static_cast<size_t>(type);
    const auto size = static_cast<int32_t>(type);

    Maze mazeInstance;
    mazeInstance.type = type;
    mazeInstance.seed = seed;
    mazeInstance.grid.resize(s, std::vector<uint8_t>(s, 1));

    std::pair<int32_t, int32_t> start{1, 1};
    mazeInstance.grid[start.first][start.second] = 0;

    std::mt19937 rng(seed);
    std::vector<std::pair<int32_t, int32_t>> walls;

    auto addWalls = [&](int32_t x, int32_t y)
    {
        if (x >= 2) walls.emplace_back(x - 2, y);
        if (y >= 2) walls.emplace_back(x, y - 2);
        if (x < size - 2) walls.emplace_back(x + 2, y);
        if (y < size - 2) walls.emplace_back(x, y + 2);
    };

    addWalls(start.first, start.second);

    // 先推一次初始状态（可选）
    if (onUpdate) onUpdate(mazeInstance);

    uint64_t carvedSteps = 0;

    while (!walls.empty())
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            break;

        std::uniform_int_distribution<size_t> wallDist(0, walls.size() - 1);
        size_t idx = wallDist(rng);

        auto [wx, wy] = walls[idx];
        walls.erase(walls.begin() + static_cast<std::ptrdiff_t>(idx));

        int32_t passages = 0;
        if (wx >= 2 && mazeInstance.grid[wx - 2][wy] == 0) passages++;
        if (wy >= 2 && mazeInstance.grid[wx][wy - 2] == 0) passages++;
        if (wx < size - 2 && mazeInstance.grid[wx + 2][wy] == 0) passages++;
        if (wy < size - 2 && mazeInstance.grid[wx][wy + 2] == 0) passages++;

        if (passages == 1)
        {
            mazeInstance.grid[wx][wy] = 0;

            if (wx >= 2 && mazeInstance.grid[wx - 2][wy] == 0)
                mazeInstance.grid[wx - 1][wy] = 0;
            else if (wy >= 2 && mazeInstance.grid[wx][wy - 2] == 0)
                mazeInstance.grid[wx][wy - 1] = 0;
            else if (wx < size - 2 && mazeInstance.grid[wx + 2][wy] == 0)
                mazeInstance.grid[wx + 1][wy] = 0;
            else if (wy < size - 2 && mazeInstance.grid[wx][wy + 2] == 0)
                mazeInstance.grid[wx][wy + 1] = 0;

            addWalls(wx, wy);

            // 可视化：每 updateEvery 次 carve 推一次
            carvedSteps++;
            if (onUpdate && updateEvery > 0 && (carvedSteps % updateEvery == 0))
            {
                onUpdate(mazeInstance); // 回调里通常是 viewer.setMaze(...)
                if (delay.count() > 0)
                    std::this_thread::sleep_for(delay);
            }
        }
    }

    // 最终再推一次完整结果（可选）
    if (onUpdate) onUpdate(mazeInstance);

    return mazeInstance;
}
