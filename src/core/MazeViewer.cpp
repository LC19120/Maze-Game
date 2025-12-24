#include "core/Common.hpp"
#include "core/MazeViewer.hpp"

MazeViewer MazeViewer::getMazeViewer(Maze maze)
{
    MazeViewer viewer;
    viewer.maze = maze;
    return viewer;
}

void MazeViewer::displayMaze() const
{
    for (const auto& row : maze.grid)
    {
        for (const auto& cell : row)
        {
            if (cell == 1)
                std::cout << '#';  // Wall
            else
                std::cout << '.';  // Path (was ' ')
        }
        std::cout << '\n';
    }
}