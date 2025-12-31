#include "core/PathFinder.hpp"

static int Heuristic(const Point& a, const Point& b)
{
    // 曼哈顿距离
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}
 //最短路径 使用A* 和 曼哈顿启发算法
std::tuple<std::vector<Point>, std::vector<Point>, int32_t, std::chrono::milliseconds>
PathFinder::pathFinder(Maze maze)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    struct Node {
        Point p;
        int g;
        int f;
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

    std::vector<Point> visitedPoints;
    std::vector<Point> path;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!openSet.empty())
    {
        Node current = openSet.top();
        openSet.pop();

        // ⭐ 记录访问节点
        visitedPoints.push_back(current.p);

        if (current.p == maze.end)
        {
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
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return {
        path,
        visitedPoints,
        static_cast<int32_t>(path.size()),
        duration
    };
}

std::tuple<std::vector<Point>,std::vector<Point> , int32_t, std::chrono::milliseconds>
WallBreaker::BreakWalls(Maze maze, int32_t breakCount)
{
    //破墙路径 使用空间BFS算法
    auto startTime = std::chrono::high_resolution_clock::now();

    struct State {
        Point p;
        int broken;
    };

    std::queue<State> q;

    auto key = [&](int x, int y, int b) {
        return b * maze.width * maze.height + y * maze.width + x;
    };

    std::unordered_set<int> visited;
    std::vector<Point> visitedPoints;
    std::unordered_map<int, State> parent;

    q.push({ maze.start, 0 });
    visited.insert(key(maze.start.x, maze.start.y, 0));

    int endKey = -1;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty())
    {
        State cur = q.front(); q.pop();

        // ⭐ 记录访问节点（去重）
        visitedPoints.push_back(cur.p);

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
        visitedPoints,
        static_cast<int32_t>(path.size()),
        duration
    };
}

//return pair<paths,lengths> , ways , time
std::tuple<std::pair<std::vector<std::vector<Point>>, std::vector<int32_t>>, int32_t, std::chrono::milliseconds>
PathCounter::CountPaths(Maze maze, Point start, Point end)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<Point>> allPaths;
    std::vector<int32_t> lengths;
    std::vector<Point> current;

    if (start == end)
    {
        allPaths.push_back({ start });
        lengths.push_back(1);
        return { { allPaths, lengths }, 1, std::chrono::milliseconds(0) };
    }

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    // 点访问标记：防止节点重复（简单路径）
    std::vector<std::vector<bool>> visited(
        maze.height, std::vector<bool>(maze.width, false));

    std::function<void(Point)> dfs = [&](Point p)
    {
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

std::tuple<std::vector<Point>, std::vector<Point>,std::vector<Point>, int32_t, std::chrono::milliseconds>
PathPasser::PassPath(Maze maze, uint32_t x, uint32_t y)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    Point mid{ static_cast<int>(x), static_cast<int>(y) };

    std::vector<Point> empty;
    if (!maze.InBounds(mid.x, mid.y) || maze.IsWall(mid.x, mid.y))
    {
        return { empty, empty, empty, 0, std::chrono::milliseconds(0) };
    }

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    auto key = [&](int x, int y) {
        return y * maze.width + x;
    };

    auto biBFS = [&](Point start, Point end,
                     std::vector<Point>& visitedOut) -> std::vector<Point>
    {
        // +++ add: trivial case
        if (start == end)
        {
            visitedOut.push_back(start);
            return { start };
        }
        // --- add

        std::queue<Point> q1, q2;
        std::unordered_map<int, Point> prev1, prev2;
        std::unordered_set<int> vis1, vis2;

        q1.push(start);
        q2.push(end);
        vis1.insert(key(start.x, start.y));
        vis2.insert(key(end.x, end.y));

        Point meet{ -1, -1 };

        while (!q1.empty() && !q2.empty())
        {
            int s1 = (int)q1.size();
            while (s1--)
            {
                Point cur = q1.front(); q1.pop();
                visitedOut.push_back(cur);

                for (int i = 0; i < 4; ++i)
                {
                    int nx = cur.x + dx[i];
                    int ny = cur.y + dy[i];
                    int k = key(nx, ny);

                    if (!maze.InBounds(nx, ny) || maze.IsWall(nx, ny)) continue;
                    if (vis1.count(k)) continue;

                    vis1.insert(k);
                    prev1[k] = cur;

                    if (vis2.count(k))
                    {
                        meet = { nx, ny };
                        goto BUILD_PATH;
                    }
                    q1.push({ nx, ny });
                }
            }

            int s2 = (int)q2.size();
            while (s2--)
            {
                Point cur = q2.front(); q2.pop();
                visitedOut.push_back(cur);

                for (int i = 0; i < 4; ++i)
                {
                    int nx = cur.x + dx[i];
                    int ny = cur.y + dy[i];
                    int k = key(nx, ny);

                    if (!maze.InBounds(nx, ny) || maze.IsWall(nx, ny)) continue;
                    if (vis2.count(k)) continue;

                    vis2.insert(k);
                    prev2[k] = cur;

                    if (vis1.count(k))
                    {
                        meet = { nx, ny };
                        goto BUILD_PATH;
                    }
                    q2.push({ nx, ny });
                }
            }
        }

    BUILD_PATH:
        if (meet.x == -1) return empty;

        std::vector<Point> path1, path2;

        Point cur = meet;
        while (!(cur == start))
        {
            path1.push_back(cur);
            cur = prev1[key(cur.x, cur.y)];
        }
        path1.push_back(start);
        std::reverse(path1.begin(), path1.end());

        cur = meet;
        while (!(cur == end))
        {
            cur = prev2[key(cur.x, cur.y)];
            path2.push_back(cur);
        }

        path1.insert(path1.end(), path2.begin(), path2.end());
        return path1;
    };

    // +++ add: if mid is start/end, it should degenerate to normal shortest path
    if (mid == maze.start)
    {
        std::vector<Point> visitedSE;
        auto pathSE = biBFS(maze.start, maze.end, visitedSE);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        return { pathSE, visitedSE, empty, (int32_t)pathSE.size(), duration };
    }
    if (mid == maze.end)
    {
        std::vector<Point> visitedSE;
        auto pathSE = biBFS(maze.start, maze.end, visitedSE);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        return { pathSE, visitedSE, empty, (int32_t)pathSE.size(), duration };
    }
    // --- add

    // ================= 执行两段 =================
    std::vector<Point> visitedSP, visitedPE;
    auto path1 = biBFS(maze.start, mid, visitedSP);
    auto path2 = biBFS(mid, maze.end, visitedPE);

    if (path1.empty() || path2.empty())
    {
        return { empty, visitedSP, visitedPE, 0,
                 std::chrono::milliseconds(0) };
    }

    path1.pop_back();
    path1.insert(path1.end(), path2.begin(), path2.end());

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return {
        path1,
        visitedSP,
        visitedPE,
        static_cast<int32_t>(path1.size()),
        duration
    };
}

