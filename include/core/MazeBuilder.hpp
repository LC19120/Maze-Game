#pragma once
#include "core/Common.hpp"

enum class MazeType
{
    Small = 51,
    Medium = 101,
    Large = 201
};

class Maze
{
public:
    MazeType type{};
    std::vector<std::vector<uint8_t>> grid{};
    int32_t seed{};
};

class MazeBuilder
{
public:
    static Maze Build(MazeType type, int32_t seed);
};