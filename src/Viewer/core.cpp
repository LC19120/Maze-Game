#include "core/Common.hpp"
#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include "core/MazeBuilder.hpp"
#include "core/PathFinder.hpp"

#include <algorithm>
#include <sstream>

// +++ add
namespace {
struct AnimPreset_ { int updateEvery; int delayMs; };

static AnimPreset_ PathPresetFor_(MazeType /*t*/)
{
    // only Medium supported now
    return { 2, 8 };
}
} // namespace
// --- add

MazeViewer& MazeViewer::getInstance()
{
    static MazeViewer instance;
    return instance;
}

MazeViewer::MazeViewer()
    : pool_(std::max(1u,
                     (std::thread::hardware_concurrency() > 2)
                         ? (std::thread::hardware_concurrency() - 2)
                         : 1u))
{
}

MazeViewer::~MazeViewer()
{
    cancelWork_();
    requestClose();
    shutdownGL_();
}

void MazeViewer::setMaze(const Maze& newMaze)
{
    {
        std::lock_guard<std::mutex> lock(mazeMutex_);
        maze_ = newMaze;
        mazeLoaded_ = true;
        mazeDirty_ = true;
    }
    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        latestMaze_ = newMaze;
        hasMaze_ = true;
    }
}

bool MazeViewer::isOpen() const
{
    if (!window_) return false;
    return glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)) == 0;
}

void MazeViewer::requestClose()
{
    closeRequested_.store(true, std::memory_order_relaxed);
}

void MazeViewer::processEvents_()
{
    glfwPollEvents();
}

void MazeViewer::renderFrame_()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &w, &h);
    fbW_ = w;
    fbH_ = h;

    glViewport(0, 0, fbW_, fbH_);
    glClearColor(0.90f, 0.90f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // --- right-side square viewport for maze (side = fbH_)
    const int side = std::min(fbW_, fbH_);
    const int mazeX = fbW_ - side;
    const int mazeY = 0;

    glViewport(mazeX, mazeY, side, side);
    drawMaze_();

    // --- restore full viewport for UI
    glViewport(0, 0, fbW_, fbH_);
    renderUi_();

    glfwSwapBuffers(static_cast<GLFWwindow*>(window_));
}

void MazeViewer::cancelWork_()
{
    std::lock_guard<std::mutex> lk(cancelMutex_);
    if (cancelFlag_) cancelFlag_->store(true, std::memory_order_relaxed);
    cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
}

void MazeViewer::updateWindowTitle_()
{
    if (!window_) return;

    std::ostringstream oss;
    oss << "Maze Viewer | "
        << "Size=Medium(71)"
        << " Seed=" << uiSeed_
        << " Start=(" << uiStartX_ << "," << uiStartY_ << ")"
        << " End=(" << uiEndX_ << "," << uiEndY_ << ")"
        << " | [B]=Build [F]=Find [ESC]=Cancel";
    glfwSetWindowTitle(static_cast<GLFWwindow*>(window_), oss.str().c_str());
}

void MazeViewer::requestBuild_(MazeType type, int32_t seed)
{
    // +++ add: set default path animation speed for this size
    {
        const auto p = PathPresetFor_(type);
        uiUpdateEvery_ = std::max(1, p.updateEvery);
        uiDelayMs_     = std::max(0, p.delayMs);
    }
    // --- add

    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Building...";
    }

    pool_.enqueue([this, type, seed, myToken] {
        try {
            // 清空成绩表统计（你已有）
            {
                std::lock_guard<std::mutex> lk(algoLenMutex_);
                algoPathLen_.fill(-1);
                algoVisited_.fill(0);
                algoFoundAt_.fill(-1);
            }
            uiAlgoIndex_ = 0;

            Maze built = MazeBuilder::Build(
                type,
                seed,
                [this, myToken](const Maze& partial) {
                    if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                    setMaze(partial);
                }
            );

            // token 过期就不要提交结果
            if (latestToken_.load(std::memory_order_relaxed) != myToken) return;

            // +++ save baseline maze (for clearing painted paths before re-run)
            {
                std::lock_guard<std::mutex> lk(latestMazeMutex_);
                baseMaze_ = built;
                hasBaseMaze_ = true;
            }
            // show final maze
            setMaze(built);
            // --- save baseline

            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = "Build done.";
        }
        catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = std::string("Build exception: ") + e.what();
        }
    });
}

void MazeViewer::requestFindPath_(int32_t sx, int32_t sy, int32_t ex, int32_t ey, int algoIndex)
{
    if (algoIndex < 0) algoIndex = 0;
    if (algoIndex > 6) algoIndex = 6;
    uiAlgoIndex_ = algoIndex;

    Maze logic{};
    Maze renderStart{};
    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        if (!hasMaze_) {
            std::lock_guard<std::mutex> lk2(uiMsgMutex_);
            uiLastMsg_ = "No maze yet. Build first.";
            return;
        }

        logic = hasBaseMaze_ ? baseMaze_ : latestMaze_;
        renderStart = latestMaze_;
        if (algoIndex == 6) renderStart = logic; // ALL 从干净图开始
    }

    // +++ FORCE endpoints: (1,1) -> (W-2,H-2)
    const int H = (int)logic.grid.size();
    const int W = (H > 0) ? (int)logic.grid[0].size() : 0;
    if (W <= 2 || H <= 2) {
        std::lock_guard<std::mutex> lk2(uiMsgMutex_);
        uiLastMsg_ = "Maze too small.";
        return;
    }
    sx = 1; sy = 1;
    ex = W - 2; ey = H - 2;
    // --- FORCE endpoints

    // +++ IMPORTANT: restore wall cells in renderStart from base (prevents BFS+ “infinite wall breaking” visually)
    if (hasBaseMaze_)
    {
        const int bH = (int)baseMaze_.grid.size();
        const int bW = (bH > 0) ? (int)baseMaze_.grid[0].size() : 0;
        if (bW == W && bH == H)
        {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (baseMaze_.grid[(size_t)y][(size_t)x] == 1)
                        renderStart.grid[(size_t)y][(size_t)x] = 1;
        }
    }
    // --- restore walls

    if (algoIndex == 6) // ALL: clear stats + clear rendering immediately
    {
        {
            std::lock_guard<std::mutex> lk(algoLenMutex_);
            algoPathLen_.fill(-1);
            algoVisited_.fill(0);
            algoFoundAt_.fill(-1);
        }
        setMaze(renderStart);
    }

    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::shared_ptr<std::atomic<bool>> myCancel;
    {
        std::lock_guard<std::mutex> lk(cancelMutex_);
        cancelFlag_->store(true, std::memory_order_relaxed);
        cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        myCancel = cancelFlag_;
    }

    const auto preset = PathPresetFor_(logic.type);
    const uint32_t updateEvery = (uint32_t)std::max(1, preset.updateEvery);

    auto delay = std::chrono::milliseconds(std::max(0, preset.delayMs));

    // algoIndex mapping: 0=DFS,1=BFS,2=BFS+,3=Dijkstra,4=A*,5=Floyd,6=ALL

    // ALL: cancel delay completely
    if (algoIndex == 6)
    {
        delay = std::chrono::milliseconds(0);
    }
    else
    {
        // BFS / Dijkstra: 2x faster
        if (algoIndex == 1 || algoIndex == 3)
            delay = std::chrono::milliseconds(delay.count() / 2);

        // BFS+: additional 2x faster (total 4x vs baseline)
        if (algoIndex == 2)
            delay = std::chrono::milliseconds(delay.count() / 8);
    }
    // --- Speed up

    pool_.enqueue([this, logic, renderStart, sx, sy, ex, ey, algoIndex, myToken, myCancel, updateEvery, delay] {
        std::vector<std::pair<int32_t, int32_t>> steps;
        std::string err;

        auto algoFromIndex = [](int i) -> PathFinder::PathAlgo {
            if (i < 0) i = 0;
            if (i > 6) i = 6;
            return (PathFinder::PathAlgo)i;
        };

        if (algoIndex == 6) // ALL
        {
            std::array<int, 6> lens{};
            std::array<int, 6> visited{};
            std::array<int, 6> foundAt{};
            std::array<std::vector<std::pair<int32_t,int32_t>>, 6> allPaths;

            const bool ok = PathFinder::FindPath(
                logic, {sx, sy}, {ex, ey},
                steps, err,
                PathFinder::PathAlgo::All,
                [this, myToken, myCancel](const Maze& animMaze) {
                    if (myCancel->load(std::memory_order_relaxed)) return;
                    if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                    setMaze(animMaze);
                },
                myCancel.get(),
                updateEvery,
                delay,
                &lens,
                &allPaths,
                &visited,
                nullptr,
                &foundAt,
                nullptr,
                &renderStart
            );

            {
                std::lock_guard<std::mutex> lk(algoLenMutex_);
                algoPathLen_ = lens;
                algoVisited_ = visited;
                algoFoundAt_ = foundAt;
            }

            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = ok ? "ALL done." : ("ALL failed: " + err);
            return;
        }

        int visitedSingle = 0;
        int foundAtSingle = -1;

        const bool ok2 = PathFinder::FindPath(
            logic, {sx, sy}, {ex, ey},
            steps, err,
            algoFromIndex(algoIndex),
            [this, myToken, myCancel](const Maze& animMaze) {
                if (myCancel->load(std::memory_order_relaxed)) return;
                if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                setMaze(animMaze);
            },
            myCancel.get(),
            updateEvery,
            delay,
            nullptr, nullptr, nullptr,
            &visitedSingle,
            nullptr,
            &foundAtSingle,
            &renderStart
        );

        if (algoIndex >= 0 && algoIndex < 6)
        {
            std::lock_guard<std::mutex> lk(algoLenMutex_);
            algoPathLen_[(size_t)algoIndex] = ok2 ? (int)steps.size() : -1;
            algoVisited_[(size_t)algoIndex] = visitedSingle;
            algoFoundAt_[(size_t)algoIndex] = foundAtSingle;
        }

        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = ok2 ? "Path done." : ("Path failed: " + err);
    });
}

void MazeViewer::run()
{
    initWindowAndGL_(); // defined in src/Viewer/window_gl.cpp

    while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)))
    {
        if (closeRequested_.load(std::memory_order_relaxed))
            glfwSetWindowShouldClose(static_cast<GLFWwindow*>(window_), GLFW_TRUE);

        processEvents_();
        renderFrame_();
    }

    shutdownGL_(); // defined in src/Viewer/window_gl.cpp
}