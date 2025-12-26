#include "core/PathFinder.hpp"

static int Heuristic(const Point& a, const Point& b)
{
    // 曼哈顿距离
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}
// return path, visted, length, time
std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
PathFinder::pathFinder(Maze maze)
{   
    //最短路径 使用A* 和 曼哈顿启发算法
    auto startTime = std::chrono::high_resolution_clock::now();

    struct Node {
        Point p;
        int g;  // cost from start
        int f;  // g + heuristic
    };

    auto cmp = [](const Node& a, const Node& b) {
        return a.f > b.f;
    };

    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> openSet(cmp);
    std::unordered_map<int, Point> cameFrom;
    std::unordered_map<int, int> costSoFar;

    auto key = [&](int x, int y) {
        return y * maze.width + x;
    };

    openSet.push({ maze.start, 0, Heuristic(maze.start, maze.end) });
    costSoFar[key(maze.start.x, maze.start.y)] = 0;

    int visited = 0;
    std::vector<Point> path;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!openSet.empty())
    {
        Node current = openSet.top();
        openSet.pop();
        visited++;

        if (current.p == maze.end)
        {
            // 回溯路径
            Point cur = maze.end;
            while (!(cur == maze.start))
            {
                path.push_back(cur);
                cur = cameFrom[key(cur.x, cur.y)];
            }
            path.push_back(maze.start);
            std::reverse(path.begin(), path.end());
            break;
        }

        for (int i = 0; i < 4; ++i)
        {
            int nx = current.p.x + dx[i];
            int ny = current.p.y + dy[i];

            if (!maze.InBounds(nx, ny) || maze.IsWall(nx, ny))
                continue;

            int newCost = current.g + 1;
            int k = key(nx, ny);

            if (!costSoFar.count(k) || newCost < costSoFar[k])
            {
                costSoFar[k] = newCost;
                int priority = newCost + Heuristic({ nx, ny }, maze.end);
                openSet.push({ { nx, ny }, newCost, priority });
                cameFrom[k] = current.p;
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return { path, visited, static_cast<int32_t>(path.size()), duration };
}

std::tuple<std::vector<Point>, int32_t, int32_t, std::chrono::milliseconds>
WallBreaker::BreakWalls(Maze maze, int32_t breakCount)
{
    //破墙路径 使用空间BFS算法
    auto startTime = std::chrono::high_resolution_clock::now();

    struct State {
        Point p;
        int broken;
    };

    std::queue<State> q;

    // key = (b * W * H) + y * W + x
    auto key = [&](int x, int y, int b) {
        return b * maze.width * maze.height + y * maze.width + x;
    };

    std::unordered_set<int> visited;
    std::unordered_map<int, State> parent;

    q.push({ maze.start, 0 });
    visited.insert(key(maze.start.x, maze.start.y, 0));

    int visitedCount = 0;
    int endKey = -1;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty())
    {
        State cur = q.front(); q.pop();
        visitedCount++;

        if (cur.p == maze.end)
        {
            endKey = key(cur.p.x, cur.p.y, cur.broken);
            break;
        }

        for (int i = 0; i < 4; ++i)
        {
            int nx = cur.p.x + dx[i];
            int ny = cur.p.y + dy[i];

            if (!maze.InBounds(nx, ny)) continue;

            int nb = cur.broken + (maze.IsWall(nx, ny) ? 1 : 0);
            if (nb > breakCount) continue;

            int nk = key(nx, ny, nb);
            if (visited.count(nk)) continue;

            visited.insert(nk);
            parent[nk] = cur;
            q.push({ { nx, ny }, nb });
        }
    }

    std::vector<Point> path;

    // 回溯路径
    if (endKey != -1)
    {
        int curKey = endKey;
        State curState = {
            maze.end,
            curKey / (maze.width * maze.height)
        };

        while (!(curState.p == maze.start && curState.broken == 0))
        {
            path.push_back(curState.p);
            curState = parent[curKey];
            curKey = key(curState.p.x, curState.p.y, curState.broken);
        }

        path.push_back(maze.start);
        std::reverse(path.begin(), path.end());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return {
        path,
        visitedCount,
        static_cast<int32_t>(path.size()),
        duration
    };
}


//return pair<paths,lengths> , ways , time
std::tuple<std::pair<std::vector<std::vector<Point>>, std::vector<int32_t>>, int32_t, std::chrono::milliseconds>
PathCounter::CountPaths(Maze maze, Point start, Point end)
{
    //多路径计数
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<Point>> allPaths;
    std::vector<int32_t> lengths;
    std::vector<Point> current;

    // 起点即终点
    if (start == end)
    {
        allPaths.push_back({ start });
        lengths.push_back(1);
        return { { allPaths, lengths }, 1, std::chrono::milliseconds(0) };
    }

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    std::vector<std::vector<bool>> visited(
        maze.height, std::vector<bool>(maze.width, false));

    std::function<void(Point)> dfs = [&](Point p)
    {
        // 进入节点
        current.push_back(p);
        visited[p.y][p.x] = true;

        if (p == end)
        {
            allPaths.push_back(current);
            lengths.push_back(static_cast<int32_t>(current.size()));
        }
        else
        {
            for (int i = 0; i < 4; ++i)
            {
                int nx = p.x + dx[i];
                int ny = p.y + dy[i];

                if (!maze.InBounds(nx, ny)) continue;
                if (maze.IsWall(nx, ny)) continue;
                if (visited[ny][nx]) continue;

                dfs({ nx, ny });
            }
        }

        // 回溯
        visited[p.y][p.x] = false;
        current.pop_back();
    };

    dfs(start);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return {
        { allPaths, lengths },
        static_cast<int32_t>(allPaths.size()),
        duration
    }; 
}
