#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"

#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    State state;
    uint32_t timeStep;
    std::vector<PointInfo> way; // points of maze way
    PointInfo StartPoint;       // start point of maze
    PointInfo EndPoint;         // end point of maze

    virtual void update();      // update a new point of maze way
    virtual ~Exploer() = default;
};


// DFS (Depth-First Search) Explorer
class DFSExploer : public Exploer
{
private:
    std::stack<PointInfo> st;                 // 显式栈：迭代 DFS 的核心
    std::unordered_set<uint32_t> visited;
    std::unordered_map<uint32_t, uint32_t> parent;

public:
    DFSExploer(Maze maze);
    void update() override;
};