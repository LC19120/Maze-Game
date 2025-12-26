#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class PathFinder
{
public:
    enum class PathAlgo : int
    {
        DFS = 0,
        BFS = 1,
        Dijkstra = 2,
        AStar = 3,
        Floyd = 4,
        All = 5
    };

    static bool FindPath(
        const Maze& maze,
        const std::pair<int32_t, int32_t>& start,
        const std::pair<int32_t, int32_t>& end,
        std::vector<std::pair<int32_t, int32_t>>& outSteps,
        std::string& outError,
        PathAlgo requestedAlgo,
        const std::function<void(const Maze&)>& onStep,
        std::atomic<bool>* cancel = nullptr,
        uint32_t updateEvery = 1,
        std::chrono::milliseconds delay = std::chrono::milliseconds{0},
        std::array<int, 5>* outAllLens = nullptr,
        std::array<std::vector<std::pair<int32_t,int32_t>>, 5>* outAllPaths = nullptr,
        std::array<int, 5>* outAllVisited = nullptr,
        int* outVisited = nullptr,
        std::array<int, 5>* outAllFoundAt = nullptr, // +++ NEW: first-hit visited index
        int* outFoundAt = nullptr                     // +++ NEW: single algo
    );
};