#include "core/PathFinder.hpp"

PathFinder::Status PathFinder::FindPath(Maze& inOutMaze,
                                        int sx, int sy, int ex, int ey,
                                        int /*algoIndex*/,
                                        std::atomic<bool>* cancel,
                                        Result* out)
{
    if (cancel && cancel->load(std::memory_order_relaxed))
        return Status::Err("Cancelled.");

    const int H = (int)inOutMaze.grid.size();
    const int W = (H > 0) ? (int)inOutMaze.grid[0].size() : 0;
    if (H <= 0 || W <= 0) return Status::Err("Empty maze.");
    if (sx < 0 || sy < 0 || ex < 0 || ey < 0 || sx >= W || ex >= W || sy >= H || ey >= H)
        return Status::Err("Out of range.");

    if (out) {
        out->visited = 0;
        out->foundAt = -1;
        out->pathLen = -1;
    }

    // Placeholder: mark start/end for now (no real search yet)
    // Avoid clobbering walls (value 1)
    if (inOutMaze.grid[sy][sx] != 1u) inOutMaze.grid[sy][sx] = 2u;
    if (inOutMaze.grid[ey][ex] != 1u) inOutMaze.grid[ey][ex] = 3u;

    return Status::Err("PathFinder not implemented yet.");
}