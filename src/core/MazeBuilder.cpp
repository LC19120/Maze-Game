#include "core/Common.hpp"

enum class MazeType
{
    Small = 50,
    Medium = 100,
    Large = 200
};

class Maze
{
public:
    MazeType type;
    std::vector<std::vector<uint8_t>> grid;
    int32_t seed;


private:
    static Maze MazeBuilder(MazeType type, int32_t seed)
    {
        size_t s = static_cast<size_t>(type);
        Maze mazeInstance;

        // Initialize Maze
        int32_t size = static_cast<int32_t>(type);
        mazeInstance.grid.resize(s, std::vector<uint8_t>(s, 1));

        std::pair<int32_t, int32_t> start{1, 1};
        std::pair<int32_t, int32_t> end{size - 2, size - 2};

        // Using Prim's Algorithm for maze generation
        {
            std::mt19937 rng(seed);
            std::uniform_int_distribution<int32_t> dist(0, 3);
            std::vector<std::pair<int32_t, int32_t>> walls;

            auto addWalls = [&](int32_t x, int32_t y) {
                if (x > 1) walls.emplace_back(x - 1, y);
                if (y > 1) walls.emplace_back(x, y - 1);
                if (x < size - 2) walls.emplace_back(x + 1, y);
                if (y < size - 2) walls.emplace_back(x, y + 1);
            };

            mazeInstance.grid[start.first][start.second] = 0;
            addWalls(start.first, start.second);

            while (!walls.empty()) {
                std::uniform_int_distribution<size_t> wallDist(0, walls.size() - 1);
                size_t idx = wallDist(rng);
                auto [wx, wy] = walls[idx];
                walls.erase(walls.begin() + idx);

                int32_t passages = 0;
                if (wx >= 2 && mazeInstance.grid[wx - 2][wy] == 0) passages++;
                if (wy >= 2 && mazeInstance.grid[wx][wy - 2] == 0) passages++;
                if (wx < size - 2 && mazeInstance.grid[wx + 2][wy] == 0) passages++;
                if (wy < size - 2 && mazeInstance.grid[wx][wy + 2] == 0) passages++;

                if (passages == 1) {
                    mazeInstance.grid[wx][wy] = 0;
                    if (wx >= 2 && mazeInstance.grid[wx - 2][wy] == 0)
                        mazeInstance.grid[wx - 1][wy] = 0;
                    else if (wy >= 2 && mazeInstance.grid[wx][wy - 2] == 0)
                        mazeInstance.grid[wx][wy - 1] = 0;
                    else if (wx < size - 2 && mazeInstance.grid[wx + 2][wy] == 0)
                        mazeInstance.grid[wx + 1][wy] = 0;
                    else if (wy < size - 2 && mazeInstance.grid[wx][wy + 2] == 0)
                        mazeInstance.grid[wx][wy + 1] = 0;
                }
            }
        }

        mazeInstance.type = type;
        mazeInstance.seed = seed;

        return mazeInstance;
    }
};