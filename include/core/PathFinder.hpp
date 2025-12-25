#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

class PathFinder
{
public:
    static bool FindPath(
        const Maze& maze,
        const std::pair<int32_t, int32_t>& start,
        const std::pair<int32_t, int32_t>& end,
        std::vector<std::pair<int32_t, int32_t>>& outSteps,
        std::string& outError
    );

    // 每一步探索后回调 onStep(maze)，用于实时渲染
    static bool FindPath(
        const Maze& maze,
        const std::pair<int32_t, int32_t>& start,
        const std::pair<int32_t, int32_t>& end,
        std::vector<std::pair<int32_t, int32_t>>& outSteps,
        std::string& outError,
        const std::function<void(const Maze&)>& onStep,
        std::atomic<bool>* cancel = nullptr,
        uint32_t updateEvery = 1,
        std::chrono::milliseconds delay = std::chrono::milliseconds{0}
    );
};