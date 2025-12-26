#include "core/PathFinder.hpp"
#include "Exploer/Exploer.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <thread>
#include <queue>         // +++
#include <random>        // +++
#include <unordered_map> // +++
#include <limits>        // +++


static bool InBounds_(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    return x >= 0 && y >= 0 && (uint32_t)x < w && (uint32_t)y < h;
}

static std::unique_ptr<Exploer> MakeExploer_(PathFinder::PathAlgo algo, const Maze& maze, std::string& outError)
{
    auto floydAllowed = [&]() { return maze.type == MazeType::Medium; };

    switch (algo)
    {
    case PathFinder::PathAlgo::DFS:      return std::make_unique<DFSExploer>(maze);
    case PathFinder::PathAlgo::BFS:      return std::make_unique<BFSExploer>(maze);
    case PathFinder::PathAlgo::BFSPlus:  return std::make_unique<BFSPlusExploer>(maze); // +++ NEW
    case PathFinder::PathAlgo::Dijkstra: return std::make_unique<DijkstraExploer>(maze);
    case PathFinder::PathAlgo::AStar:    return std::make_unique<AStarExploer>(maze);
    case PathFinder::PathAlgo::Floyd:
        if (!floydAllowed()) { outError = "Floyd is only allowed on Medium(71)."; return nullptr; }
        return std::make_unique<FloydExploer>(maze);
    case PathFinder::PathAlgo::All:      return std::make_unique<AllExploer>(maze);
    default:
        outError = "Unknown algorithm.";
        return nullptr;
    }
}

bool PathFinder::FindPath(
    const Maze& maze,
    const std::pair<int32_t, int32_t>& start,
    const std::pair<int32_t, int32_t>& end,
    std::vector<std::pair<int32_t, int32_t>>& outSteps,
    std::string& outError,
    PathAlgo requestedAlgo,
    const std::function<void(const Maze&)>& onStep,
    std::atomic<bool>* cancel,
    uint32_t updateEvery,
    std::chrono::milliseconds delay,
    std::array<int, 6>* outAllLens,
    std::array<std::vector<std::pair<int32_t,int32_t>>, 6>* outAllPaths,
    std::array<int, 6>* outAllVisited,
    int* outVisited,
    std::array<int, 6>* outAllFoundAt,
    int* outFoundAt,
    const Maze* renderStartMaze)
{
    outSteps.clear();
    outError.clear();

    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;
    if (W == 0 || H == 0) { outError = "Maze grid is empty."; return false; }
    if (!InBounds_(start.first, start.second, W, H)) { outError = "Start out of bounds."; return false; }
    if (!InBounds_(end.first, end.second, W, H)) { outError = "End out of bounds."; return false; }

    // ---- REMOVE Floyd fallback: always run the requested algo
    PathAlgo runAlgo = requestedAlgo;
    // ---- REMOVE block:
    // const uint64_t cells = (uint64_t)W * (uint64_t)H;
    // if (requestedAlgo == PathAlgo::Floyd && cells > 160 * 160) { ... }

    auto exploer = MakeExploer_(runAlgo, maze, outError);
    if (!exploer) return false;

    // +++ FIX: pass start/end/cancel into exploer
    exploer->cancel = cancel;
    exploer->StartPoint = {
        (uint32_t)start.first,
        (uint32_t)start.second,
        0u,
        0.0f
    };
    exploer->EndPoint = {
        (uint32_t)end.first,
        (uint32_t)end.second,
        0u,
        0.0f
    };
    exploer->state = State::START;
    exploer->timeStep = 0;
    exploer->way.clear();
    exploer->path.clear();
    exploer->found = false;
    exploer->error.clear();
    // --- FIX

    // color code must match BUTTON
    auto codeFor = [&](PathAlgo a) -> uint8_t
    {
        switch (a)
        {
        case PathAlgo::DFS:      return 2;
        case PathAlgo::BFS:      return 3;
        case PathAlgo::BFSPlus:  return 7;
        case PathAlgo::Dijkstra: return 4;
        case PathAlgo::AStar:    return 5;
        case PathAlgo::Floyd:    return 6;
        default:                 return 2;
        }
    };
    auto exploreCodeFor = [&](PathAlgo a) -> uint8_t { return (uint8_t)(codeFor(a) + 10); }; // 12..17

    constexpr uint8_t kBfsPlusWallPathCode = 27; // +++ NEW: wall cell with 50% overlay square

    Maze anim = (renderStartMaze ? *renderStartMaze : maze);

    auto paintExplore = [&](uint32_t x, uint32_t y, uint8_t exploreCode, bool overwrite)
    {
        if (y >= anim.grid.size() || x >= anim.grid[y].size()) return;

        // +++ IMPORTANT: NEVER paint walls during explore (including BFS+)
        if (maze.grid[y][x] == 1) return;
        // --- IMPORTANT

        if (overwrite) anim.grid[y][x] = exploreCode;
        else { if (anim.grid[y][x] == 0) anim.grid[y][x] = exploreCode; }
    };

    auto paintPathOverwriteNormal = [&](const std::vector<PointInfo>& p, uint8_t code)
    {
        for (const auto& pt : p)
        {
            if (pt.y >= anim.grid.size() || pt.x >= anim.grid[pt.y].size()) continue;
            if (maze.grid[pt.y][pt.x] == 1) continue; // normal algos never draw on walls
            anim.grid[pt.y][pt.x] = code;
        }
    };

    // +++ BFS+: draw path; on open cells => full cell color; on wall cells => special 27 (render as 50% square on wall)
    auto paintPathOverwriteBfsPlus = [&](const std::vector<PointInfo>& p)
    {
        for (const auto& pt : p)
        {
            if (pt.y >= anim.grid.size() || pt.x >= anim.grid[pt.y].size()) continue;

            if (maze.grid[pt.y][pt.x] == 1) anim.grid[pt.y][pt.x] = kBfsPlusWallPathCode;
            else                            anim.grid[pt.y][pt.x] = codeFor(PathAlgo::BFSPlus);
        }
    };
    // --- BFS+

    auto paintPathFirstCome = [&](const std::vector<PointInfo>& p, PathAlgo algo)
    {
        const uint8_t code = codeFor(algo);

        for (const auto& pt : p)
        {
            if (pt.y >= anim.grid.size() || pt.x >= anim.grid[pt.y].size()) continue;

            // BFS+ special: allow painting wall-overlay cells (27) but don't overwrite other final paths
            if (algo == PathAlgo::BFSPlus && maze.grid[pt.y][pt.x] == 1)
            {
                const uint8_t cur = anim.grid[pt.y][pt.x];
                const bool emptyOrVisited = (cur == 0) || (cur >= 12 && cur <= 17);
                if (emptyOrVisited) anim.grid[pt.y][pt.x] = kBfsPlusWallPathCode;
                continue;
            }

            // others: don't touch walls
            if (maze.grid[pt.y][pt.x] == 1) continue;

            const uint8_t cur = anim.grid[pt.y][pt.x];
            const bool emptyOrVisited = (cur == 0) || (cur >= 12 && cur <= 17);
            if (emptyOrVisited) anim.grid[pt.y][pt.x] = code;
        }
    };

    std::array<size_t, 6> paintedAllEach{{0,0,0,0,0,0}};
    size_t paintedSingle = 0;
    size_t paintedTotalAll = 0;

    const uint32_t maxSteps = (runAlgo == PathAlgo::All) ? (W * H * 64u) : (W * H * 32u);
    uint32_t guard = 0;

    int foundAtSingle = -1;
    std::array<int, 6> foundAtAll;
    foundAtAll.fill(-1);

    while (exploer->state != State::END)
    {
        if (cancel && cancel->load(std::memory_order_relaxed)) { outError = "Cancelled."; return false; }

        exploer->update();
        if (++guard > maxSteps) { outError = "Search exceeded step limit."; return false; }

        // +++ capture first time reaching end ("point first arrived")
        if (requestedAlgo != PathAlgo::All)
        {
            if (foundAtSingle < 0 && exploer->found)
                foundAtSingle = (int)exploer->way.size();
        }
        else
        {
            auto* all = dynamic_cast<AllExploer*>(exploer.get());
            if (all)
            {
                for (int ai = 0; ai < 6; ++ai)
                {
                    auto* a = all->algos_[(size_t)ai].get();
                    if (!a) continue;
                    if (foundAtAll[(size_t)ai] < 0 && a->found)
                        foundAtAll[(size_t)ai] = (int)a->way.size();
                }
            }
        }
        // --- capture

        if (runAlgo != PathAlgo::All)
        {
            const uint8_t exploreCode = exploreCodeFor(requestedAlgo);

            if (exploer->way.size() > paintedSingle)
            {
                for (size_t i = paintedSingle; i < exploer->way.size(); ++i)
                    paintExplore(exploer->way[i].x, exploer->way[i].y, exploreCode, /*overwrite=*/true);

                paintedSingle = exploer->way.size();

                if (onStep && updateEvery > 0 && (paintedSingle % updateEvery == 0))
                {
                    onStep(anim);
                    if (delay.count() > 0) std::this_thread::sleep_for(delay);
                }
            }
        }
        else
        {
            auto* all = dynamic_cast<AllExploer*>(exploer.get());
            if (!all) { outError = "ALL: exploer type mismatch."; return false; }

            for (int ai = 0; ai < 6; ++ai)
            {
                auto* a = all->algos_[(size_t)ai].get();
                if (!a) continue;

                const auto algo = (PathAlgo)ai;               // 0..5 matches enum
                const uint8_t exploreCode = exploreCodeFor(algo);

                if (a->way.size() > paintedAllEach[(size_t)ai])
                {
                    for (size_t i = paintedAllEach[(size_t)ai]; i < a->way.size(); ++i)
                        paintExplore(a->way[i].x, a->way[i].y, exploreCode, /*overwrite=*/false);

                    paintedTotalAll += (a->way.size() - paintedAllEach[(size_t)ai]);
                    paintedAllEach[(size_t)ai] = a->way.size();
                }
            }

            if (onStep && updateEvery > 0 && (paintedTotalAll % updateEvery == 0))
            {
                onStep(anim);
                if (delay.count() > 0) std::this_thread::sleep_for(delay);
            }
        }
    }

    // ---- paint final paths
    if (runAlgo != PathAlgo::All)
    {
        if (requestedAlgo == PathAlgo::BFSPlus) paintPathOverwriteBfsPlus(exploer->path);
        else paintPathOverwriteNormal(exploer->path, codeFor(requestedAlgo));
    }
    else
    {
        auto* all = dynamic_cast<AllExploer*>(exploer.get());
        if (all)
        {
            for (int ai = 0; ai < 6; ++ai)
            {
                auto* a = all->algos_[(size_t)ai].get();
                if (!a || !a->found || a->path.empty()) continue;

                const auto algo = (PathAlgo)ai;
                if (algo == PathAlgo::BFSPlus)
                {
                    // BFS+ in ALL: prefer first-come for open cells, but wall-cells become 27 overlays
                    paintPathFirstCome(a->path, PathAlgo::BFSPlus);
                }
                else
                {
                    paintPathFirstCome(a->path, algo);
                }
            }
        }
    }

    if (onStep) onStep(anim);

    // visited output
    if (outVisited) *outVisited = (int)exploer->way.size();
    if (outFoundAt) *outFoundAt = foundAtSingle;
    if (requestedAlgo == PathAlgo::All && outAllFoundAt) *outAllFoundAt = foundAtAll;

    // fill per-algo lens + paths (+ visited) when ALL requested
    if (requestedAlgo == PathAlgo::All)
    {
        auto* all = dynamic_cast<AllExploer*>(exploer.get());

        if (outAllLens) outAllLens->fill(-1);
        if (outAllPaths) for (auto& v : *outAllPaths) v.clear();
        if (outAllVisited) outAllVisited->fill(0);

        if (all)
        {
            for (int ai = 0; ai < 6; ++ai)
            {
                auto* a = all->algos_[(size_t)ai].get();
                if (!a) continue;

                if (outAllLens)
                    (*outAllLens)[(size_t)ai] = (a->found && !a->path.empty()) ? (int)a->path.size() : -1;

                if (outAllVisited)
                    (*outAllVisited)[(size_t)ai] = (int)a->way.size();

                if (outAllPaths && a->found && !a->path.empty())
                {
                    auto& dst = (*outAllPaths)[(size_t)ai];
                    dst.reserve(a->path.size());
                    for (const auto& p : a->path)
                        dst.emplace_back((int32_t)p.x, (int32_t)p.y);
                }
            }
        }
    }

    if (!exploer->found || exploer->path.empty())
    {
        if (exploer->error.size()) outError = exploer->error;
        if (outError.empty()) outError = "No path found.";
        return false;
    }

    outSteps.reserve(exploer->path.size());
    for (const auto& p : exploer->path)
        outSteps.emplace_back((int32_t)p.x, (int32_t)p.y);

    return true;
}

static bool Walkable_(const Maze& m, int x, int y)
{
    if (y < 0 || x < 0) return false;
    if ((size_t)y >= m.grid.size()) return false;
    if ((size_t)x >= m.grid[(size_t)y].size()) return false;
    return m.grid[(size_t)y][(size_t)x] != 1;
}

static std::vector<int> BfsDist_(const Maze& m, int sx, int sy)
{
    const int H = (int)m.grid.size();
    const int W = (H > 0) ? (int)m.grid[0].size() : 0;

    std::vector<int> dist((size_t)W * (size_t)H, -1);
    if (W <= 0 || H <= 0) return dist;
    if (!Walkable_(m, sx, sy)) return dist;

    auto idx = [&](int x, int y) { return (size_t)y * (size_t)W + (size_t)x; };

    std::queue<std::pair<int,int>> q;
    dist[idx(sx,sy)] = 0;
    q.push({sx,sy});

    const int dx4[4] = {1,-1,0,0};
    const int dy4[4] = {0,0,1,-1};

    while (!q.empty())
    {
        auto [x,y] = q.front(); q.pop();
        const int d = dist[idx(x,y)];
        for (int k=0;k<4;++k)
        {
            const int nx = x + dx4[k];
            const int ny = y + dy4[k];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (!Walkable_(m, nx, ny)) continue;
            if (dist[idx(nx,ny)] >= 0) continue;
            dist[idx(nx,ny)] = d + 1;
            q.push({nx,ny});
        }
    }
    return dist;
}

// sample K different shortest paths (same length), diversified by "used edges"
static std::vector<std::vector<std::pair<uint32_t,uint32_t>>> SampleKShortest_(
    const Maze& m, int sx, int sy, int ex, int ey, int K, uint32_t seed)
{
    std::vector<std::vector<std::pair<uint32_t,uint32_t>>> out;
    if (K <= 0) return out;

    const int H = (int)m.grid.size();
    const int W = (H > 0) ? (int)m.grid[0].size() : 0;
    if (W <= 0 || H <= 0) return out;

    auto dist = BfsDist_(m, sx, sy);
    auto idx = [&](int x, int y) { return (size_t)y * (size_t)W + (size_t)x; };

    if (sx < 0 || sy < 0 || ex < 0 || ey < 0 || sx >= W || ex >= W || sy >= H || ey >= H) return out;
    const int targetD = dist[idx(ex,ey)];
    if (targetD < 0) return out;

    const int dx4[4] = {1,-1,0,0};
    const int dy4[4] = {0,0,1,-1};

    std::unordered_map<uint64_t, int> used;
    used.reserve(8192);

    auto edgeKey = [&](int ax,int ay,int bx,int by)->uint64_t{
        const uint32_t a = (uint32_t)idx(ax,ay);
        const uint32_t b = (uint32_t)idx(bx,by);
        const uint32_t lo = std::min(a,b), hi = std::max(a,b);
        return (uint64_t)lo << 32 | (uint64_t)hi;
    };

    std::mt19937 rng(seed);

    for (int pi = 0; pi < K; ++pi)
    {
        std::vector<std::pair<uint32_t,uint32_t>> path;
        path.reserve((size_t)targetD + 1);

        int x = sx, y = sy;
        path.push_back({(uint32_t)x,(uint32_t)y});

        std::array<int,4> ord{{0,1,2,3}};
        std::shuffle(ord.begin(), ord.end(), rng);

        bool ok = true;
        for (int step = 0; step < targetD; ++step)
        {
            int bestNx = 0, bestNy = 0;
            int bestCost = std::numeric_limits<int>::max();
            bool has = false;

            for (int t=0;t<4;++t)
            {
                const int k = ord[(size_t)t];
                const int nx = x + dx4[k];
                const int ny = y + dy4[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (!Walkable_(m, nx, ny)) continue;

                const int nd = dist[idx(nx,ny)];
                if (nd != dist[idx(x,y)] + 1) continue; // stay on shortest DAG

                const int c = used[edgeKey(x,y,nx,ny)];
                if (!has || c < bestCost) { bestCost = c; bestNx = nx; bestNy = ny; has = true; }
            }

            if (!has) { ok = false; break; }

            used[edgeKey(x,y,bestNx,bestNy)] += 1;
            x = bestNx; y = bestNy;
            path.push_back({(uint32_t)x,(uint32_t)y});
        }

        if (!ok || x != ex || y != ey) continue;
        out.push_back(std::move(path));
    }

    return out;
}