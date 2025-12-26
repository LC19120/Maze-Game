#pragma once
#include "core/Common.hpp"
#include "core/DataStruct.hpp"

class PathFinder
{
    public:
        static std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
        pathFinder(Maze maze);
};

class WallBreaker : public PathFinder
{
    public:
        static std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
        BreakWalls(Maze maze, int32_t breakCount);

};

class PathCounter : public PathFinder
{
    public:
        static std::tuple<std::pair<std::vector<std::vector<Point>>, std::vector<int32_t>>, int32_t, std::chrono::milliseconds>
        CountPaths(Maze maze, Point start, Point end);

};