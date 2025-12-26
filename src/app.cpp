#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "Viewer/core.hpp"
#include "core/PathFinder.hpp"


void runApp()
{
    auto& viewer = MazeViewer::getInstance();
    viewer.run();
}