#include "core/Common.hpp"
#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include "core/MazeBuilder.hpp"
#include "core/PathFinder.hpp"

#include <algorithm>
#include <sstream>

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
        << "Type=" << (uiTypeIndex_ + 1)
        << " Seed=" << uiSeed_
        << " Start=(" << uiStartX_ << "," << uiStartY_ << ")"
        << " End=(" << uiEndX_ << "," << uiEndY_ << ")"
        << " | [B]=Build [F]=Find [ESC]=Cancel";
    glfwSetWindowTitle(static_cast<GLFWwindow*>(window_), oss.str().c_str());
}

void MazeViewer::requestBuild_(MazeType type, int32_t seed)
{
    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::shared_ptr<std::atomic<bool>> myCancel;
    {
        std::lock_guard<std::mutex> lk(cancelMutex_);
        cancelFlag_->store(true, std::memory_order_relaxed);
        cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        myCancel = cancelFlag_;
    }

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Building...";
    }

    pool_.enqueue([this, type, seed, myToken, myCancel] {
        try {
            MazeBuilder::Build(
                type,
                seed,
                [this, myToken, myCancel](const Maze& partial) {
                    if (myCancel->load(std::memory_order_relaxed)) return;
                    if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                    setMaze(partial);
                },
                myCancel.get(),
                40,
                std::chrono::milliseconds(8)
            );

            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = myCancel->load(std::memory_order_relaxed) ? "Build cancelled." : "Build done.";
        }
        catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = std::string("Build exception: ") + e.what();
        }
    });
}

void MazeViewer::requestFindPath_(int32_t sx, int32_t sy, int32_t ex, int32_t ey)
{
    Maze snapshot{};
    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        if (!hasMaze_) {
            std::lock_guard<std::mutex> lk2(uiMsgMutex_);
            uiLastMsg_ = "No maze yet. Build first.";
            return;
        }
        snapshot = latestMaze_;
    }

    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::shared_ptr<std::atomic<bool>> myCancel;
    {
        std::lock_guard<std::mutex> lk(cancelMutex_);
        cancelFlag_->store(true, std::memory_order_relaxed);
        cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        myCancel = cancelFlag_;
    }

    const uint32_t updateEvery = (uiUpdateEvery_ <= 0) ? 1u : (uint32_t)uiUpdateEvery_;
    const auto delay = std::chrono::milliseconds(std::max(0, uiDelayMs_));

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Finding path...";
    }

    pool_.enqueue([this, snapshot, sx, sy, ex, ey, myToken, myCancel, updateEvery, delay] {
        std::vector<std::pair<int32_t, int32_t>> steps;
        std::string err;

        const bool ok = PathFinder::FindPath(
            snapshot,
            {sx, sy},
            {ex, ey},
            steps,
            err,
            [this, myToken, myCancel](const Maze& animMaze) {
                if (myCancel->load(std::memory_order_relaxed)) return;
                if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                setMaze(animMaze);
            },
            myCancel.get(),
            updateEvery,
            delay
        );

        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        if (myCancel->load(std::memory_order_relaxed)) uiLastMsg_ = "Path cancelled.";
        else uiLastMsg_ = ok ? "Path done." : ("Path failed: " + err);
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