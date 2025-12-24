#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

class MazeViewer
{
public:
    MazeViewer getMazeViewer(Maze maze);
    void displayMaze() const;
private:
    Maze maze;
    
};