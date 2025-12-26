#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp" // add: for Maze

#include <atomic>   // add
#include <memory>   // add
#include <string>   // add

#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <array>

struct PointInfo
{
    uint32_t x, y;
    uint32_t step;
    float distance;
};

enum class State
{
    START,
    EXPLORE,
    END
};

class Exploer
{
public:
    Maze maze;
    State state{State::START};
    uint32_t timeStep{0};

    std::vector<PointInfo> way;   // for animation: explored order (monotonic grow)
    std::vector<PointInfo> path;  // final path: start -> end

    PointInfo StartPoint{0,0,0,0.0f};
    PointInfo EndPoint{0,0,0,0.0f};

    bool found{false};
    std::string error;

    std::atomic<bool>* cancel{nullptr}; // set by PathFinder

    virtual void update();
    virtual ~Exploer() = default;
};

// DFS
class DFSExploer : public Exploer
{
private:
    std::stack<PointInfo> st;
    std::unordered_set<uint32_t> visited;
    std::unordered_map<uint32_t, uint32_t> parent;

public:
    DFSExploer(Maze maze);
    void update() override;
};

// BFS
class BFSExploer : public Exploer
{
private:
    std::queue<PointInfo> q;
    std::unordered_set<uint32_t> visited;
    std::unordered_map<uint32_t, uint32_t> parent;

public:
    BFSExploer(Maze maze);
    void update() override;
};

// BFS+
class BFSPlusExploer : public Exploer
{
private:
    struct SNode { uint32_t x, y; uint8_t b; uint32_t step; };
    std::queue<SNode> q;
    std::unordered_set<uint32_t> visited;
    std::unordered_map<uint32_t, uint32_t> parent;

public:
    BFSPlusExploer(Maze maze);
    void update() override;
};

// +++ add: Dijkstra
class DijkstraExploer : public Exploer
{
private:
    struct Node { int32_t d; uint32_t x; uint32_t y; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.d > b.d; } };

    std::priority_queue<Node, std::vector<Node>, Cmp> pq;
    std::unordered_map<uint32_t, int32_t> dist;
    std::unordered_map<uint32_t, uint32_t> parent;
    std::unordered_set<uint32_t> closed;

public:
    DijkstraExploer(Maze maze);
    void update() override;
};

// +++ add: A*
class AStarExploer : public Exploer
{
private:
    struct Node { int32_t f; int32_t g; uint32_t x; uint32_t y; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };

    std::priority_queue<Node, std::vector<Node>, Cmp> pq;
    std::unordered_map<uint32_t, int32_t> gScore;
    std::unordered_map<uint32_t, uint32_t> parent;
    std::unordered_set<uint32_t> closed;

public:
    AStarExploer(Maze maze);
    void update() override;
};

// +++ add: Floyd (compressed graph, then animate final path)
class FloydExploer : public Exploer
{
private:
    bool computed_{false};
    size_t animIdx_{0};

public:
    FloydExploer(Maze maze);
    void update() override;
};

// +++ change: ALL (runs all algos in sync)
class AllExploer : public Exploer
{
public:
    // 0..5 => DFS,BFS,BFS+,DIJKSTRA,A*,FLOYD
    std::array<std::unique_ptr<Exploer>, 6> algos_{}; // was 5

    AllExploer(Maze maze);
    void update() override;
};
// --- change