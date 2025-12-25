#include "core/Common.hpp"
#include "Exploer/Exploer.hpp"

// +++ add: base virtual function definition to satisfy vtable emission
void Exploer::update() {
    // base no-op
}
// --- add

// DFS Exploer
DFSExploer::DFSExploer(Maze maze){
    this->maze = maze;
    this->state = State::START;
    this->timeStep = 0;
    this->way.clear();
    this->StartPoint = {0, 0, 0, 0.0f};
    this->EndPoint = {0, 0, 0, 0.0f};

    while (!st.empty()) st.pop();
    visited.clear();
    parent.clear();
}

static uint32_t keyOf(uint32_t x, uint32_t y, uint32_t W) {
    return y * W + x;
}

void DFSExploer::update(){
    const uint32_t HEIGHT = (uint32_t)maze.grid.size();
    const uint32_t WIDTH  = (HEIGHT > 0) ? (uint32_t)maze.grid[0].size() : 0;

    auto inBounds = [&](int x, int y) -> bool {
        return x >= 0 && y >= 0 && (uint32_t)x < WIDTH && (uint32_t)y < HEIGHT;
    };

    auto isWalkable = [&](uint32_t x, uint32_t y) -> bool {
        // grid[y][x] == 0 可走；1 是墙
        return maze.grid[y][x] == 0;
    };

    if(state == State::START){
        this->state = State::EXPLORE;
        this->timeStep = 1;

        this->way.clear();
        while (!st.empty()) st.pop();
        visited.clear();
        parent.clear();

        st.push(this->StartPoint);
        visited.insert(keyOf(StartPoint.x, StartPoint.y, WIDTH));

        return;
    }
    else if(state == State::EXPLORE){
        this->timeStep += 1;

        // 每次 update 推进一步（弹一个点并扩展邻居）
        if (st.empty()) {
            this->state = State::END; // 没路了
            return;
        }

        PointInfo cur = st.top();
        st.pop();

        // 记录探索顺序（如果你想显示探索过程）
        this->way.push_back(cur);

        // 到终点就结束
        if (cur.x == EndPoint.x && cur.y == EndPoint.y) {
            this->state = State::END;
            return;
        }

        // 邻居：下、右、上、左（不递归 DFS）
        const int dx[4] = {0, 1, 0, -1};
        const int dy[4] = {1, 0, -1, 0};

        // 注意：想保持特定顺序的话，入栈顺序会影响 DFS 访问顺序
        for (int i = 3; i >= 0; --i) {
            int nx = (int)cur.x + dx[i];
            int ny = (int)cur.y + dy[i];

            if (!inBounds(nx, ny)) continue;

            uint32_t ux = (uint32_t)nx;
            uint32_t uy = (uint32_t)ny;

            if (!isWalkable(ux, uy)) continue;

            uint32_t nk = keyOf(ux, uy, WIDTH);
            if (visited.count(nk)) continue;

            visited.insert(nk);
            parent[nk] = keyOf(cur.x, cur.y, WIDTH);

            st.push({ux, uy, cur.step + 1, 0.0f});
        }
    }
    else if(state == State::END){
        // 你可以在这里做：回溯 parent 生成最终路径、或停止更新
    }
}