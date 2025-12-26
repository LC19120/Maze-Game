#include "core/PathFinder.hpp"


// return path, visted, length, time
std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
PathFinder::pathFinder(Maze maze)
{   
    //最短路径 使用A* 和 曼哈顿启发算法
}

std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
WallBreaker::BreakWalls(Maze maze, int32_t breakCount)
{
    //破墙路径 使用空间BFS算法

}

//return pair<paths,lengths> , ways , time
std::tuple<std::pair<std::vector<std::vector<Point>>, std::vector<int32_t>>, int32_t, std::chrono::milliseconds>
PathCounter::CountPaths(Maze maze, Point start, Point end)
{
    //多路径计数
    
}