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
        BFSPlus = 2,   // +++ NEW
        Dijkstra = 3,
        AStar = 4,
        Floyd = 5,
        All = 6
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
        std::array<int, 6>* outAllLens = nullptr, // was 5
        std::array<std::vector<std::pair<int32_t,int32_t>>, 6>* outAllPaths = nullptr, // was 5
        std::array<int, 6>* outAllVisited = nullptr, // was 5
        int* outVisited = nullptr,
        std::array<int, 6>* outAllFoundAt = nullptr, // was 5
        int* outFoundAt = nullptr,
        const Maze* renderStartMaze = nullptr         // +++ NEW: allow overlay rendering
    );
};