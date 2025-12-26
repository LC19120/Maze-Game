#include "core/Common.hpp"
#include "Exploer/Exploer.hpp"

#include <limits>
#include <unordered_map>   // +++
#include <vector>          // +++
#include <algorithm>       // +++
#include <string>          // +++


void Exploer::update() {}

// key helpers
static uint32_t keyOf(uint32_t x, uint32_t y, uint32_t W) { return y * W + x; }
static std::pair<uint32_t,uint32_t> xyOf(uint32_t k, uint32_t W) { return {k % W, k / W}; }

static bool cancelled_(Exploer* e)
{
    return e && e->cancel && e->cancel->load(std::memory_order_relaxed);
}

static void buildPathFromParent_(uint32_t startK,
                                 uint32_t endK,
                                 uint32_t W,
                                 const std::unordered_map<uint32_t,uint32_t>& parent,
                                 std::vector<PointInfo>& outPath)
{
    outPath.clear();
    uint32_t cur = endK;

    // if end == start allow
    // was: if (cur != startK && !parent.contains(cur)) return;
    if (cur != startK && parent.find(cur) == parent.end()) return;

    // reverse collect
    std::vector<uint32_t> rev;
    rev.reserve(256);
    rev.push_back(cur);

    while (cur != startK)
    {
        auto it = parent.find(cur);
        if (it == parent.end()) { outPath.clear(); return; }
        cur = it->second;
        rev.push_back(cur);
        if (rev.size() > 5'000'000) { outPath.clear(); return; }
    }

    std::reverse(rev.begin(), rev.end());
    uint32_t step = 0;
    for (uint32_t k : rev)
    {
        auto [x,y] = xyOf(k, W);
        outPath.push_back({x,y,step++,0.0f});
    }
}

// ---------------- DFS ----------------
DFSExploer::DFSExploer(Maze maze){
    this->maze = maze;
    this->state = State::START;
    this->timeStep = 0;
    this->way.clear();
    this->path.clear();
    this->found = false;
    this->error.clear();
    this->StartPoint = {0, 0, 0, 0.0f};
    this->EndPoint   = {0, 0, 0, 0.0f};

    while (!st.empty()) st.pop();
    visited.clear();
    parent.clear();
}

void DFSExploer::update(){
    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < W && (uint32_t)y < H;
    };
    auto isWalkable = [&](uint32_t x, uint32_t y) -> bool {
        return maze.grid[y][x] != 1; // FIX: allow painted cells (2..6) as walkable
    };

    if(state == State::START){
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();
        while (!st.empty()) st.pop();
        visited.clear();
        parent.clear();

        if (!isWalkable(StartPoint.x, StartPoint.y) || !isWalkable(EndPoint.x, EndPoint.y)) {
            state = State::END; error = "Start/End is wall."; return;
        }

        st.push(StartPoint);
        visited.insert(keyOf(StartPoint.x, StartPoint.y, W));
        return;
    }

    if(state != State::EXPLORE) return;

    timeStep += 1;

    if (st.empty()) {
        state = State::END;
        found = false;
        error = "No path.";
        return;
    }

    PointInfo cur = st.top();
    st.pop();

    way.push_back(cur);

    const uint32_t curK = keyOf(cur.x, cur.y, W);
    const uint32_t endK = keyOf(EndPoint.x, EndPoint.y, W);
    const uint32_t startK = keyOf(StartPoint.x, StartPoint.y, W);

    if (curK == endK) {
        buildPathFromParent_(startK, endK, W, parent, path);
        found = !path.empty();
        state = State::END;
        return;
    }

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {1, 0, -1, 0};

    for (int i = 3; i >= 0; --i) {
        int nx = (int)cur.x + dx[i];
        int ny = (int)cur.y + dy[i];

        if (!inBounds(nx, ny)) continue;

        uint32_t ux = (uint32_t)nx;
        uint32_t uy = (uint32_t)ny;

        if (!isWalkable(ux, uy)) continue;

        uint32_t nk = keyOf(ux, uy, W);
        if (visited.count(nk)) continue;

        visited.insert(nk);
        parent[nk] = curK;

        st.push({ux, uy, cur.step + 1, 0.0f});
    }
}

// ---------------- BFS ----------------
BFSExploer::BFSExploer(Maze maze) {
    this->maze = maze;
    this->state = State::START;
    this->timeStep = 0;
    this->way.clear();
    this->path.clear();
    this->found = false;
    this->error.clear();
    this->StartPoint = {0, 0, 0, 0.0f};
    this->EndPoint   = {0, 0, 0, 0.0f};

    while (!q.empty()) q.pop();
    visited.clear();
    parent.clear();
}

void BFSExploer::update() {
    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < W && (uint32_t)y < H;
    };
    auto isWalkable = [&](uint32_t x, uint32_t y) -> bool {
        return maze.grid[y][x] != 1; // FIX
    };

    const uint32_t startK = keyOf(StartPoint.x, StartPoint.y, W);
    const uint32_t endK   = keyOf(EndPoint.x, EndPoint.y, W);

    if (state == State::START) {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();
        while (!q.empty()) q.pop();
        visited.clear();
        parent.clear();

        if (!isWalkable(StartPoint.x, StartPoint.y) || !isWalkable(EndPoint.x, EndPoint.y)) {
            state = State::END; error = "Start/End is wall."; return;
        }

        q.push(StartPoint);
        visited.insert(startK);
        return;
    }

    if (state != State::EXPLORE) return;

    timeStep += 1;

    if (q.empty()) {
        state = State::END;
        found = false;
        error = "No path.";
        return;
    }

    PointInfo cur = q.front();
    q.pop();

    way.push_back(cur);

    const uint32_t curK = keyOf(cur.x, cur.y, W);
    if (curK == endK) {
        buildPathFromParent_(startK, endK, W, parent, path);
        found = !path.empty();
        state = State::END;
        return;
    }

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {1, 0, -1, 0};

    for (int i = 0; i < 4; ++i) {
        int nx = (int)cur.x + dx[i];
        int ny = (int)cur.y + dy[i];
        if (!inBounds(nx, ny)) continue;

        uint32_t ux = (uint32_t)nx;
        uint32_t uy = (uint32_t)ny;
        if (!isWalkable(ux, uy)) continue;

        uint32_t nk = keyOf(ux, uy, W);
        if (visited.count(nk)) continue;

        visited.insert(nk);
        parent[nk] = curK;
        q.push({ux, uy, cur.step + 1, 0.0f});
    }
}

// ---------------- Dijkstra ----------------
DijkstraExploer::DijkstraExploer(Maze maze)
{
    this->maze = maze;
    this->state = State::START;
}

void DijkstraExploer::update()
{
    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < W && (uint32_t)y < H;
    };
    auto isWalkable = [&](uint32_t x, uint32_t y) -> bool {
        return maze.grid[y][x] != 1; // FIX
    };

    const uint32_t startK = keyOf(StartPoint.x, StartPoint.y, W);
    const uint32_t endK   = keyOf(EndPoint.x, EndPoint.y, W);

    if (state == State::START)
    {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();

        while (!pq.empty()) pq.pop();
        dist.clear(); parent.clear(); closed.clear();

        if (!isWalkable(StartPoint.x, StartPoint.y) || !isWalkable(EndPoint.x, EndPoint.y)) {
            state = State::END; error = "Start/End is wall."; return;
        }

        dist[startK] = 0;
        pq.push({0, StartPoint.x, StartPoint.y});
        return;
    }

    if (state != State::EXPLORE) return;

    timeStep += 1;

    // settle one node per update
    while (!pq.empty())
    {
        const Node cur = pq.top();
        pq.pop();
        const uint32_t ck = keyOf(cur.x, cur.y, W);
        if (closed.count(ck)) continue;

        closed.insert(ck);
        way.push_back({cur.x, cur.y, (uint32_t)cur.d, (float)cur.d});

        if (ck == endK)
        {
            buildPathFromParent_(startK, endK, W, parent, path);
            found = !path.empty();
            state = State::END;
            return;
        }

        const int dx[4] = {0,1,0,-1};
        const int dy[4] = {1,0,-1,0};

        for (int i = 0; i < 4; ++i)
        {
            const int nx = (int)cur.x + dx[i];
            const int ny = (int)cur.y + dy[i];
            if (!inBounds(nx, ny)) continue;

            const uint32_t ux = (uint32_t)nx;
            const uint32_t uy = (uint32_t)ny;
            if (!isWalkable(ux, uy)) continue;

            const uint32_t nk = keyOf(ux, uy, W);
            if (closed.count(nk)) continue;

            const int32_t nd = cur.d + 1;
            auto it = dist.find(nk);
            if (it == dist.end() || nd < it->second)
            {
                dist[nk] = nd;
                parent[nk] = ck;
                pq.push({nd, ux, uy});
            }
        }
        return; // processed 1 settled node
    }

    state = State::END;
    found = false;
    error = "No path.";
}

// ---------------- A* ----------------
AStarExploer::AStarExploer(Maze maze)
{
    this->maze = maze;
    this->state = State::START;
}

void AStarExploer::update()
{
    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < W && (uint32_t)y < H;
    };
    auto isWalkable = [&](uint32_t x, uint32_t y) -> bool {
        return maze.grid[y][x] != 1; // FIX
    };
    auto h = [&](uint32_t x, uint32_t y) -> int32_t {
        return (int32_t)std::abs((int32_t)x - (int32_t)EndPoint.x) + (int32_t)std::abs((int32_t)y - (int32_t)EndPoint.y);
    };

    const uint32_t startK = keyOf(StartPoint.x, StartPoint.y, W);
    const uint32_t endK   = keyOf(EndPoint.x, EndPoint.y, W);

    if (state == State::START)
    {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();

        while (!pq.empty()) pq.pop();
        gScore.clear(); parent.clear(); closed.clear();

        if (!isWalkable(StartPoint.x, StartPoint.y) || !isWalkable(EndPoint.x, EndPoint.y)) {
            state = State::END; error = "Start/End is wall."; return;
        }

        gScore[startK] = 0;
        pq.push({h(StartPoint.x, StartPoint.y), 0, StartPoint.x, StartPoint.y});
        return;
    }

    if (state != State::EXPLORE) return;

    timeStep += 1;

    while (!pq.empty())
    {
        const Node cur = pq.top();
        pq.pop();

        const uint32_t ck = keyOf(cur.x, cur.y, W);
        if (closed.count(ck)) continue;
        closed.insert(ck);

        way.push_back({cur.x, cur.y, (uint32_t)cur.g, (float)cur.g});

        if (ck == endK)
        {
            buildPathFromParent_(startK, endK, W, parent, path);
            found = !path.empty();
            state = State::END;
            return;
        }

        const int dx[4] = {0,1,0,-1};
        const int dy[4] = {1,0,-1,0};

        for (int i = 0; i < 4; ++i)
        {
            const int nx = (int)cur.x + dx[i];
            const int ny = (int)cur.y + dy[i];
            if (!inBounds(nx, ny)) continue;

            const uint32_t ux = (uint32_t)nx;
            const uint32_t uy = (uint32_t)ny;
            if (!isWalkable(ux, uy)) continue;

            const uint32_t nk = keyOf(ux, uy, W);
            if (closed.count(nk)) continue;

            const int32_t ng = cur.g + 1;
            auto it = gScore.find(nk);
            if (it == gScore.end() || ng < it->second)
            {
                gScore[nk] = ng;
                parent[nk] = ck;
                pq.push({ng + h(ux, uy), ng, ux, uy});
            }
        }
        return; // settle 1 node per update
    }

    state = State::END;
    found = false;
    error = "No path.";
}

// ---------------- Floyd (compressed) ----------------
FloydExploer::FloydExploer(Maze maze)
{
    this->maze = maze;
    this->state = State::START;
}

static int degWalk_(const Maze& m, int x, int y)
{
    const int H = (int)m.grid.size();
    const int W = (H > 0) ? (int)m.grid[0].size() : 0;
    auto inb = [&](int xx,int yy){ return xx>=0 && yy>=0 && xx<W && yy<H; };
    auto ok  = [&](int xx,int yy){ return m.grid[yy][xx] != 1; }; // FIX
    if (!inb(x,y) || !ok(x,y)) return 0;
    int d=0;
    d += inb(x+1,y)&&ok(x+1,y);
    d += inb(x-1,y)&&ok(x-1,y);
    d += inb(x,y+1)&&ok(x,y+1);
    d += inb(x,y-1)&&ok(x,y-1);
    return d;
}

static int FloydMaxNodes_(MazeType /*t*/)
{
    // only Medium exists now
    return 1800;
}

void FloydExploer::update()
{
    const int H = (int)maze.grid.size();
    const int W = (H > 0) ? (int)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inb = [&](int x,int y){ return x>=0 && y>=0 && x<W && y<H; };
    auto ok  = [&](int x,int y){ return maze.grid[y][x] != 1; }; // FIX

    if (state == State::START)
    {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();
        computed_ = false;
        animIdx_ = 0 ;

        if (!ok((int)StartPoint.x,(int)StartPoint.y) || !ok((int)EndPoint.x,(int)EndPoint.y)) {
            state = State::END; error = "Start/End is wall."; return;
        }

        return;
    }

    if (state != State::EXPLORE) return;

    // compute once, then animate path cell-by-cell
    if (!computed_)
    {
        // build compressed nodes
        auto isNode = [&](int x,int y)->bool{
            if (!ok(x,y)) return false;
            if (x==(int)StartPoint.x && y==(int)StartPoint.y) return true;
            if (x==(int)EndPoint.x   && y==(int)EndPoint.y)   return true;
            return degWalk_(maze,x,y) != 2;
        };

        std::vector<std::pair<int,int>> nodes;
        nodes.reserve((size_t)W*(size_t)H/4);

        std::unordered_map<int,int> nodeId;
        nodeId.reserve((size_t)W*(size_t)H/4);

        for (int y=0;y<H;++y)
        for (int x=0;x<W;++x)
        {
            if (!isNode(x,y)) continue;
            const int cid = y*W + x;
            nodeId[cid] = (int)nodes.size();
            nodes.push_back({x,y});
        }

        auto getNode = [&](int x,int y)->int{
            auto it = nodeId.find(y*W + x);
            return (it==nodeId.end()) ? -1 : it->second;
        };

        const int sN = getNode((int)StartPoint.x,(int)StartPoint.y);
        const int eN = getNode((int)EndPoint.x,(int)EndPoint.y);
        if (sN < 0 || eN < 0) { state = State::END; error = "Floyd: node map failed."; return; }

        const int n = (int)nodes.size();

        const int maxN = FloydMaxNodes_(maze.type);

        const uint64_t nn = (uint64_t)n * (uint64_t)n;
        const uint64_t bytes = nn * sizeof(int) * 2ull;

        if (n > maxN) {
            state = State::END;
            error = "Floyd: graph too large (n=" + std::to_string(n) + ", limit=" + std::to_string(maxN) + ").";
            return;
        }
        if (bytes > 512ull * 1024ull * 1024ull) {
            state = State::END;
            error = "Floyd: memory too large for dist/next.";
            return;
        }

        // +++ define matrices + corridor graph
        const int INF = 1'000'000'000;

        std::vector<int> dist;
        std::vector<int> next;
        dist.assign((size_t)n * (size_t)n, INF);
        next.assign((size_t)n * (size_t)n, -1);

        auto D = [&](int i, int j) -> int& {
            return dist[(size_t)i * (size_t)n + (size_t)j];
        };
        auto N = [&](int i, int j) -> int& {
            return next[(size_t)i * (size_t)n + (size_t)j];
        };

        auto keyUV = [&](int u, int v) -> uint64_t {
            return (uint64_t)(uint32_t)u << 32 | (uint32_t)v;
        };

        std::unordered_map<uint64_t, std::vector<std::pair<uint32_t,uint32_t>>> corridor;
        corridor.reserve((size_t)n * 4);

        // init diagonal
        for (int i = 0; i < n; ++i) {
            D(i,i) = 0;
            N(i,i) = i;
        }

        // build edges by FOLLOWING corridors (handles turns)
        const int dx4[4] = {1,-1,0,0};
        const int dy4[4] = {0,0,1,-1};

        auto isNodeCell = [&](int x, int y) -> bool {
            if (!ok(x,y)) return false;
            if (x==(int)StartPoint.x && y==(int)StartPoint.y) return true;
            if (x==(int)EndPoint.x   && y==(int)EndPoint.y)   return true;
            return degWalk_(maze,x,y) != 2;
        };

        auto neighborsWalkable = [&](int x, int y, int px, int py, int& outNx, int& outNy) -> int {
            // returns count of candidates (excluding prev cell), and gives one candidate
            int cnt = 0;
            int candX = 0, candY = 0;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + dx4[d];
                const int ny = y + dy4[d];
                if (!inb(nx,ny) || !ok(nx,ny)) continue;
                if (nx == px && ny == py) continue;
                candX = nx; candY = ny;
                ++cnt;
            }
            outNx = candX; outNy = candY;
            return cnt;
        };

        for (int u = 0; u < n; ++u)
        {
            const int ux = nodes[(size_t)u].first;
            const int uy = nodes[(size_t)u].second;

            for (int dir = 0; dir < 4; ++dir)
            {
                int x = ux + dx4[dir];
                int y = uy + dy4[dir];
                if (!inb(x,y) || !ok(x,y)) continue;

                int px = ux, py = uy; // previous cell (start at node u)

                std::vector<std::pair<uint32_t,uint32_t>> pathCells;
                pathCells.reserve(128);

                // walk until hit a node (following turns)
                while (true)
                {
                    if (!inb(x,y) || !ok(x,y)) { pathCells.clear(); break; }

                    pathCells.push_back({(uint32_t)x, (uint32_t)y});

                    if (isNodeCell(x,y))
                        break; // reached node v (inclusive)

                    int nx = 0, ny = 0;
                    const int cnt = neighborsWalkable(x, y, px, py, nx, ny);
                    if (cnt <= 0) { pathCells.clear(); break; } // dead end unexpectedly
                    // if cnt>1, it's a junction -> should have been a node; still pick one to avoid crash
                    px = x; py = y;
                    x = nx; y = ny;
                }

                if (pathCells.empty()) continue;

                const int vx = (int)pathCells.back().first;
                const int vy = (int)pathCells.back().second;
                const int v = getNode(vx, vy);
                if (v < 0 || v == u) continue;

                const int len = (int)pathCells.size();
                if (len < D(u,v)) {
                    D(u,v) = len;
                    N(u,v) = v;
                    corridor[keyUV(u,v)] = pathCells;
                }

                // reverse corridor v->u : exclude v, include u
                std::vector<std::pair<uint32_t,uint32_t>> rev;
                rev.reserve(pathCells.size());
                for (int idx = (int)pathCells.size() - 2; idx >= 0; --idx)
                    rev.push_back(pathCells[(size_t)idx]);
                rev.push_back({(uint32_t)ux, (uint32_t)uy});

                if (len < D(v,u)) {
                    D(v,u) = len;
                    N(v,u) = u;
                    corridor[keyUV(v,u)] = std::move(rev);
                }
            }
        }
        // --- matrices + corridor graph

        // floyd-warshall
        for (int k=0;k<n;++k)
        {
            if (cancelled_(this)) { state=State::END; error="Cancelled."; return; }
            for (int i=0;i<n;++i)
            {
                const int dik = D(i,k);
                if (dik >= INF) continue;
                for (int j=0;j<n;++j)
                {
                    const int dkj = D(k,j);
                    if (dkj >= INF) continue;
                    const int nd = dik + dkj;
                    if (nd < D(i,j))
                    {
                        D(i,j) = nd;
                        N(i,j) = N(i,k);
                    }
                }
            }
        }

        if (D(sN,eN) >= INF || N(sN,eN) < 0) { state=State::END; error="No path."; return; }

        // reconstruct node path
        std::vector<int> nodePath;
        nodePath.reserve(256);
        int cur = sN;
        nodePath.push_back(cur);
        while (cur != eN)
        {
            const int nxt = N(cur, eN);
            if (nxt < 0) break;
            cur = nxt;
            nodePath.push_back(cur);
            if ((int)nodePath.size() > n + 5) break;
        }
        if (nodePath.back() != eN) { state=State::END; error="Floyd reconstruct failed."; return; }

        // expand to cell path
        path.clear();
        path.push_back({StartPoint.x, StartPoint.y, 0u, 0.0f});
        uint32_t step=1;

        for (size_t i=0;i+1<nodePath.size();++i)
        {
            const int u=nodePath[i], v=nodePath[i+1];
            auto it = corridor.find(keyUV(u,v));
            if (it == corridor.end()) { state=State::END; error="Floyd corridor missing."; return; }
            for (auto [cx,cy] : it->second)
                path.push_back({cx,cy,step++,0.0f});
        }

        found = !path.empty();
        computed_ = true;
        animIdx_ = 0;
        return;
    }

    // animate along final path
    if (animIdx_ < path.size())
    {
        way.push_back(path[animIdx_++]);
        return;
    }

    state = State::END;
}

// ---------------- BFS+ (break walls up to n=3) ----------------
BFSPlusExploer::BFSPlusExploer(Maze maze)
{
    this->maze = maze;
    this->state = State::START;
    this->timeStep = 0;
    this->way.clear();
    this->path.clear();
    this->found = false;
    this->error.clear();
    this->StartPoint = {0, 0, 0, 0.0f};
    this->EndPoint   = {0, 0, 0, 0.0f};

    while (!q.empty()) q.pop();
    visited.clear();
    parent.clear();
}

static void buildPathFromParent3D_(uint32_t startK,
                                  uint32_t endK,
                                  uint32_t W,
                                  uint32_t H,
                                  const std::unordered_map<uint32_t,uint32_t>& parent,
                                  std::vector<PointInfo>& outPath)
{
    outPath.clear();
    uint32_t cur = endK;

    if (cur != startK && parent.find(cur) == parent.end()) return;

    std::vector<uint32_t> rev;
    rev.reserve(512);
    rev.push_back(cur);

    while (cur != startK)
    {
        auto it = parent.find(cur);
        if (it == parent.end()) { outPath.clear(); return; }
        cur = it->second;
        rev.push_back(cur);
        if (rev.size() > 5'000'000) { outPath.clear(); return; }
    }

    std::reverse(rev.begin(), rev.end());

    const uint32_t base = W * H;
    uint32_t step = 0;
    for (uint32_t k3 : rev)
    {
        const uint32_t cell = k3 % base;
        const uint32_t x = cell % W;
        const uint32_t y = cell / W;
        outPath.push_back({x, y, step++, 0.0f});
    }
}

void BFSPlusExploer::update()
{
    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { state = State::END; error = "Empty grid."; return; }
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < W && (uint32_t)y < H;
    };

    constexpr uint8_t kMaxBreak = 3;
    const uint32_t base = W * H;
    auto key3 = [&](uint32_t x, uint32_t y, uint8_t b) -> uint32_t {
        return (uint32_t)b * base + keyOf(x, y, W);
    };

    const uint32_t startK = key3(StartPoint.x, StartPoint.y, 0);
    const uint32_t endCell = keyOf(EndPoint.x, EndPoint.y, W);

    if (state == State::START)
    {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear(); path.clear(); found = false; error.clear();
        while (!q.empty()) q.pop();
        visited.clear(); parent.clear();

        q.push({StartPoint.x, StartPoint.y, 0, 0});
        visited.insert(startK);
        return;
    }

    if (state != State::EXPLORE) return;
    timeStep += 1;

    if (q.empty()) { state = State::END; found = false; error = "No path."; return; }

    const SNode cur = q.front();
    q.pop();

    way.push_back({cur.x, cur.y, cur.step, (float)cur.step});

    if (keyOf(cur.x, cur.y, W) == endCell)
    {
        const uint32_t endK = key3(cur.x, cur.y, cur.b);
        buildPathFromParent3D_(startK, endK, W, H, parent, path);
        found = !path.empty();
        state = State::END;
        return;
    }

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {1, 0, -1, 0};

    for (int i = 0; i < 4; ++i)
    {
        const int nx = (int)cur.x + dx[i];
        const int ny = (int)cur.y + dy[i];
        if (!inBounds(nx, ny)) continue;

        const uint32_t ux = (uint32_t)nx;
        const uint32_t uy = (uint32_t)ny;

        const bool isWall = (maze.grid[uy][ux] == 1);
        const uint8_t nb = (uint8_t)(cur.b + (isWall ? 1 : 0));
        if (nb > kMaxBreak) continue;

        const uint32_t nk = key3(ux, uy, nb);
        if (visited.count(nk)) continue;

        visited.insert(nk);
        parent[nk] = key3(cur.x, cur.y, cur.b);
        q.push({ux, uy, nb, cur.step + 1});
    }
}

// ---------------- ALL (sync-run all algos) ----------------
AllExploer::AllExploer(Maze maze)
{
    this->maze = maze;
    this->state = State::START;
}

void AllExploer::update()
{
    if (cancelled_(this)) { state = State::END; error = "Cancelled."; return; }

    if (state == State::START)
    {
        state = State::EXPLORE;
        timeStep = 1;

        way.clear();
        path.clear();
        found = false;
        error.clear();

        algos_[0] = std::make_unique<DFSExploer>(maze);
        algos_[1] = std::make_unique<BFSExploer>(maze);
        algos_[2] = std::make_unique<BFSPlusExploer>(maze); // +++ NEW
        algos_[3] = std::make_unique<DijkstraExploer>(maze);
        algos_[4] = std::make_unique<AStarExploer>(maze);

        const bool floydAllowed = (maze.type == MazeType::Medium);
        if (floydAllowed) algos_[5] = std::make_unique<FloydExploer>(maze);
        else algos_[5].reset();

        for (auto& a : algos_)
        {
            if (!a) continue;
            a->cancel = this->cancel;
            a->StartPoint = this->StartPoint;
            a->EndPoint   = this->EndPoint;
            a->state = State::START;
            a->found = false;
            a->error.clear();
            a->way.clear();
            a->path.clear();
        }
        return;
    }

    if (state != State::EXPLORE) return;
    timeStep += 1;

    bool allEnded = true;
    for (auto& a : algos_)
    {
        if (!a) continue;
        if (a->state != State::END) a->update();
        if (a->state != State::END) allEnded = false;
    }
    if (!allEnded) return;

    size_t bestLen = (size_t)-1;
    Exploer* best = nullptr;
    for (auto& a : algos_)
    {
        if (!a) continue;
        if (a->found && !a->path.empty() && a->path.size() < bestLen)
        {
            bestLen = a->path.size();
            best = a.get();
        }
    }

    if (best) { found = true; path = best->path; }
    else { found = false; error = "No path."; }

    state = State::END;
}