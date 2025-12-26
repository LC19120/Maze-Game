#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include "core/PathFinder.hpp"

#include <stdexcept>

MazeViewer& MazeViewer::getInstance()
{
    static MazeViewer inst;
    return inst;
}

MazeViewer::MazeViewer()
{
    // UI expects -1 to mean "no result"
    algoPathLen_.fill(-1);
    algoVisited_.fill(0);
    algoFoundAt_.fill(-1);

    uiLastMsg_ = "Ready.";
}

void MazeViewer::updateWindowTitle_()
{
    GLFWwindow* w = static_cast<GLFWwindow*>(window_);
    if (!w) return;

    std::string msg;
    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        msg = uiLastMsg_;
    }

    if (msg.empty()) msg = "Maze Viewer";
    glfwSetWindowTitle(w, msg.c_str());
}

void MazeViewer::cancelWork_()
{
    cancel_.store(true, std::memory_order_relaxed);
}

void MazeViewer::requestBuild_(int seed)
{
    cancel_.store(false, std::memory_order_relaxed);

    Maze m = MazeBuilder::Build(seed);

    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        latestMaze_ = m;
        hasMaze_ = true;
    }

    {
        std::lock_guard<std::mutex> lk(mazeMutex_);
        maze_ = m;
        mazeLoaded_ = true;
        mazeDirty_ = true;
    }

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Built.";
    }
    updateWindowTitle_();
}

void MazeViewer::requestFindPath_(int sx, int sy, int ex, int ey, int algoIndex)
{
    uiAlgoIndex_ = algoIndex;
    cancel_.store(false, std::memory_order_relaxed);

    Maze snapshot{};
    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        if (!hasMaze_) {
            std::lock_guard<std::mutex> lk2(uiMsgMutex_);
            uiLastMsg_ = "No maze yet. Click BUILD first.";
            updateWindowTitle_();
            return;
        }
        snapshot = latestMaze_;
    }

    // Minimal: call PathFinder stub (you can replace with real algorithms later)
    PathFinder::Result res{};
    auto st = PathFinder::FindPath(snapshot, sx, sy, ex, ey, algoIndex, &cancel_, &res);

    // update algo stats panel (only 0..5 are shown there)
    if (algoIndex >= 0 && algoIndex < 6)
    {
        std::lock_guard<std::mutex> lk(algoLenMutex_);
        algoPathLen_[(size_t)algoIndex] = res.pathLen;
        algoVisited_[(size_t)algoIndex] = res.visited;
        algoFoundAt_[(size_t)algoIndex] = res.foundAt;
    }

    {
        std::lock_guard<std::mutex> lk(mazeMutex_);
        maze_ = snapshot;   // allow PathFinder to paint grid
        mazeLoaded_ = true;
        mazeDirty_ = true;
    }

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = st.ok ? "Done." : st.message;
    }
    updateWindowTitle_();
}

void MazeViewer::run()
{
    initWindowAndGL_();

    if (!hasMaze_) {
        requestBuild_(uiSeed_);
    }

    GLFWwindow* win = static_cast<GLFWwindow*>(window_);
    if (!win) return;

    while (!glfwWindowShouldClose(win))
    {
        glfwGetFramebufferSize(win, &fbW_, &fbH_);
        if (fbW_ <= 0) fbW_ = 1;
        if (fbH_ <= 0) fbH_ = 1;
        glViewport(0, 0, fbW_, fbH_);

        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        drawMaze_();
        renderUi_();

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    shutdownGL_();
}