#pragma once
#include "core/Common.hpp"

struct Point
{
    int32_t x;
    int32_t y;
    std::string info;

    bool operator==(const Point& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct Maze{
    std::vector<std::vector<int32_t>> grid{};
    int32_t seed{};
    std::string info;
    int32_t width, height;
    Point start, end;

    bool InBounds(int x, int y) const {
        return x >= 0 && y >= 0 && y < (int32_t)grid.size() && x < (int32_t)grid[0].size();
    }

    bool IsWall(int x, int y) const {
        return InBounds(x, y) ? grid[y][x] == 1 : true;
    }
    
};