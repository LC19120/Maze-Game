#pragma once
#include "core/Common.hpp"
#include "core/DataStruct.hpp"

class PathFinder
{
public:
    struct Status
    {
        bool ok{false};
        std::string message;
        static Status Ok() { return {true, {}}; }
        static Status Err(std::string m) { return {false, std::move(m)}; }
    };

    struct Result
    {
        int pathLen{-1};
        int visited{0};
        int foundAt{-1};
    };

    // Minimal unified entry; algoIndex: 0..6 (ALL=6)
    static Status FindPath(Maze& inOutMaze,
                           int sx, int sy, int ex, int ey,
                           int algoIndex,
                           std::atomic<bool>* cancel,
                           Result* out);
};