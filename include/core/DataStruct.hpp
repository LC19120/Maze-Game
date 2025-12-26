#pragma once
#include "core/Common.hpp"


struct Maze{
    std::vector<std::vector<int32_t>> grid{};
    int32_t seed{};
    std::string info;
};

struct Point
{
    int32_t x;
    int32_t y;
    std::string info;
};
