#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

Maze MazeBuilder::Build(
    MazeType type,
    int32_t seed,
    const std::function<void(const Maze&)>& onUpdate)
{
    const auto s = static_cast<size_t>(type);
    const auto size = static_cast<int32_t>(type);

    Maze mazeInstance;
    mazeInstance.type = type;
    mazeInstance.seed = seed;

    // grid[y][x], 1 = wall, 0 = road
    mazeInstance.grid.assign(s, std::vector<uint8_t>(s, 1));

    auto inb = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && x < size && y < size;
    };
    auto interior = [&](int x, int y) -> bool {
        return x >= 1 && y >= 1 && x <= size - 2 && y <= size - 2;
    };

    auto openCell = [&](int x, int y) {
        if (!inb(x, y)) return;
        uint8_t& v = mazeInstance.grid[(size_t)y][(size_t)x];
        v = 0;
    };

    std::mt19937 rng(seed);

    if (onUpdate) onUpdate(mazeInstance);

    // ------------------------------------------------------------
    // 1) Perfect maze via iterative DFS (recursive backtracker)
    // ------------------------------------------------------------
    struct Cell { int x, y; };
    std::vector<Cell> st;
    st.reserve((size_t)size * (size_t)size / 4);

    const int sx = 1, sy = 1;
    openCell(sx, sy);
    st.push_back({sx, sy});

    const int dx4[4] = { 2, -2,  0,  0 };
    const int dy4[4] = { 0,  0,  2, -2 };

    while (!st.empty())
    {
        const Cell cur = st.back();

        Cell cand[4];
        int cnt = 0;

        for (int d = 0; d < 4; ++d)
        {
            const int nx = cur.x + dx4[d];
            const int ny = cur.y + dy4[d];
            if (!interior(nx, ny)) continue;
            if (mazeInstance.grid[(size_t)ny][(size_t)nx] == 1)
                cand[cnt++] = {nx, ny};
        }

        if (cnt == 0) { st.pop_back(); continue; }

        std::uniform_int_distribution<int> pick(0, cnt - 1);
        const Cell nxt = cand[pick(rng)];

        // carve wall between
        const int wx = (cur.x + nxt.x) / 2;
        const int wy = (cur.y + nxt.y) / 2;
        openCell(wx, wy);
        openCell(nxt.x, nxt.y);

        st.push_back(nxt);
    }

    // ------------------------------------------------------------
    // 2) Carve some "rooms" (more routes)
    // ------------------------------------------------------------
    // 适度 rooms：太多/太大会让 Floyd 压缩节点暴涨
    const int roomCount = 10;   // was 8
    const int maxHalf   = 3;   // was 3 (smaller rooms)

    std::uniform_int_distribution<int> cxDist(2, size - 3);
    std::uniform_int_distribution<int> cyDist(2, size - 3);
    std::uniform_int_distribution<int> halfDist(1, std::max(1, maxHalf));

    auto isOpen = [&](int x, int y) -> bool {
        return inb(x,y) && mazeInstance.grid[(size_t)y][(size_t)x] == 0;
    };

    for (int i = 0; i < roomCount; ++i)
    {
        // pick a center that is already connected to maze passages
        int cx = 1, cy = 1;
        for (int tries = 0; tries < 500; ++tries)
        {
            cx = cxDist(rng);
            cy = cyDist(rng);
            if (!interior(cx, cy)) continue;
            if (isOpen(cx, cy)) break;
        }

        // keep room aligned to odd coords (optional but helps aesthetics)
        if ((cx & 1) == 0) cx += (cx < size - 2) ? 1 : -1;
        if ((cy & 1) == 0) cy += (cy < size - 2) ? 1 : -1;

        const int hx = halfDist(rng);
        const int hy = halfDist(rng);

        const int x0 = std::max(1, cx - hx);
        const int x1 = std::min(size - 2, cx + hx);
        const int y0 = std::max(1, cy - hy);
        const int y1 = std::min(size - 2, cy + hy);

        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                openCell(x, y);

        // ensure at least one doorway to outside open cell
        bool connected = false;
        for (int y = y0; y <= y1 && !connected; ++y)
        {
            for (int x = x0; x <= x1 && !connected; ++x)
            {
                const bool boundary = (x == x0 || x == x1 || y == y0 || y == y1);
                if (!boundary) continue;

                if (isOpen(x + 1, y) || isOpen(x - 1, y) || isOpen(x, y + 1) || isOpen(x, y - 1))
                    connected = true;
            }
        }

        if (!connected)
        {
            // carve a simple doorway outward from a random boundary point
            std::uniform_int_distribution<int> side(0, 3);
            for (int tries = 0; tries < 50 && !connected; ++tries)
            {
                int x = x0, y = y0;
                const int s = side(rng);
                if (s == 0) { x = x0; y = std::uniform_int_distribution<int>(y0, y1)(rng); }
                if (s == 1) { x = x1; y = std::uniform_int_distribution<int>(y0, y1)(rng); }
                if (s == 2) { y = y0; x = std::uniform_int_distribution<int>(x0, x1)(rng); }
                if (s == 3) { y = y1; x = std::uniform_int_distribution<int>(x0, x1)(rng); }

                // try opening one step outward
                const int dx = (x == x0) ? -1 : (x == x1) ? 1 : 0;
                const int dy = (y == y0) ? -1 : (y == y1) ? 1 : 0;
                if (dx == 0 && dy == 0) continue;

                const int ox = x + dx;
                const int oy = y + dy;
                if (!interior(ox, oy)) continue;

                openCell(ox, oy);
                connected = true;
            }
        }
    }

    // ------------------------------------------------------------
    // 3) "Braid" (remove some walls between open cells => loops)
    // ------------------------------------------------------------
    auto braidProb = [&]() -> float {
        // Medium only: add loops so there are multiple routes
        return 0.06f; // was 0.00f, tune 0.04~0.09
    };

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    const float p = braidProb();

    for (int y = 1; y <= size - 2; ++y)
    {
        for (int x = 1; x <= size - 2; ++x)
        {
            if (mazeInstance.grid[(size_t)y][(size_t)x] != 1) continue;

            const bool xOdd = (x & 1) != 0;
            const bool yOdd = (y & 1) != 0;
            if (xOdd == yOdd) continue;

            if (prob(rng) > p) continue;

            if (!xOdd && yOdd)
            {
                if (mazeInstance.grid[(size_t)y][(size_t)(x - 1)] == 0 &&
                    mazeInstance.grid[(size_t)y][(size_t)(x + 1)] == 0)
                {
                    openCell(x, y);
                }
            }
            else if (xOdd && !yOdd)
            {
                if (mazeInstance.grid[(size_t)(y - 1)][(size_t)x] == 0 &&
                    mazeInstance.grid[(size_t)(y + 1)][(size_t)x] == 0)
                {
                    openCell(x, y);
                }
            }
        }
    }

    // Ensure default start/end are open
    openCell(1, 1);
    openCell(size - 2, size - 2);

    if (onUpdate) onUpdate(mazeInstance);
    return mazeInstance;
}
