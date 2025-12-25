#include "core/PathFinder.hpp"
#include "Exploer/Exploer.hpp"

static bool InBounds(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    return x >= 0 && y >= 0 && (uint32_t)x < w && (uint32_t)y < h;
}

bool PathFinder::FindPath(
    const Maze& maze,
    const std::pair<int32_t, int32_t>& start,
    const std::pair<int32_t, int32_t>& end,
    std::vector<std::pair<int32_t, int32_t>>& outSteps,
    std::string& outError
) {
    return FindPath(maze, start, end, outSteps, outError, {}, nullptr, 1, std::chrono::milliseconds{0});
}

bool PathFinder::FindPath(
    const Maze& maze,
    const std::pair<int32_t, int32_t>& start,
    const std::pair<int32_t, int32_t>& end,
    std::vector<std::pair<int32_t, int32_t>>& outSteps,
    std::string& outError,
    const std::function<void(const Maze&)>& onStep,
    std::atomic<bool>* cancel,
    uint32_t updateEvery,
    std::chrono::milliseconds delay
) {
    outSteps.clear();
    outError.clear();

    const uint32_t H = (uint32_t)maze.grid.size();
    const uint32_t W = (H > 0) ? (uint32_t)maze.grid[0].size() : 0;

    if (W == 0 || H == 0) { outError = "Maze grid is empty."; return false; }
    if (!InBounds(start.first, start.second, W, H)) { outError = "Start out of bounds."; return false; }
    if (!InBounds(end.first, end.second, W, H)) { outError = "End out of bounds."; return false; }

    DFSExploer exploer(maze);
    exploer.StartPoint = {(uint32_t)start.first, (uint32_t)start.second, 0u, 0.0f};
    exploer.EndPoint   = {(uint32_t)end.first,   (uint32_t)end.second,   0u, 0.0f};
    exploer.state = State::START;

    // 动画用的迷宫副本：2 = explored
    Maze anim = maze;
    auto paint = [&](uint32_t x, uint32_t y) {
        if (y < anim.grid.size() && x < anim.grid[y].size()) {
            if (anim.grid[y][x] == 0) anim.grid[y][x] = 2;
        }
    };

    size_t painted = 0;

    const uint32_t maxSteps = W * H * 8u;
    uint32_t guard = 0;

    while (exploer.state != State::END) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            outError = "Cancelled.";
            return false;
        }

        exploer.update();

        if (++guard > maxSteps) {
            outError = "Search exceeded step limit (check walkable/visited).";
            return false;
        }

        if (exploer.way.size() > painted) {
            for (size_t i = painted; i < exploer.way.size(); ++i) {
                paint(exploer.way[i].x, exploer.way[i].y);
            }
            painted = exploer.way.size();

            if (onStep && updateEvery > 0 && (painted % updateEvery == 0)) {
                onStep(anim);
                if (delay.count() > 0) std::this_thread::sleep_for(delay);
            }
        }
    }

    outSteps.reserve(exploer.way.size());
    bool reachedEnd = false;
    for (const auto& p : exploer.way) {
        outSteps.emplace_back((int32_t)p.x, (int32_t)p.y);
        if (p.x == exploer.EndPoint.x && p.y == exploer.EndPoint.y) reachedEnd = true;
    }

    if (!reachedEnd) { outError = "No path found."; return false; }

    if (onStep) onStep(anim); // 最终再推一次
    return true;
}