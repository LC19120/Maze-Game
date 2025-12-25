#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>
#include <sstream>

void MazeViewer::applyEdit_()
{
    auto parseInt = [&](int& dst)
    {
        if (uiEdit_.empty() || uiEdit_ == "-") return;
        try { dst = std::stoi(uiEdit_); } catch (...) {}
    };

    switch (uiFocus_)
    {
    case UiField::Seed:        parseInt(uiSeed_); break;
    case UiField::StartX:      parseInt(uiStartX_); break;
    case UiField::StartY:      parseInt(uiStartY_); break;
    case UiField::EndX:        parseInt(uiEndX_); break;
    case UiField::EndY:        parseInt(uiEndY_); break;
    case UiField::UpdateEvery: parseInt(uiUpdateEvery_); break;
    case UiField::DelayMs:     parseInt(uiDelayMs_); break;
    default: break;
    }
}

void MazeViewer::initUiCallbacks_()
{
    GLFWwindow* win = static_cast<GLFWwindow*>(window_);
    glfwSetWindowUserPointer(win, this);

    glfwSetCharCallback(win, [](GLFWwindow* w, unsigned int codepoint)
    {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (self->uiFocus_ == UiField::None) return;

        if (codepoint > 127) return;
        const char ch = (char)codepoint;
        if (ch >= '0' && ch <= '9') {
            if (self->uiEdit_.size() < 12) self->uiEdit_.push_back(ch);
        } else if (ch == '-') {
            if (self->uiEdit_.empty()) self->uiEdit_.push_back(ch);
        }
    });

    glfwSetKeyCallback(win, [](GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/)
    {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

        if (key == GLFW_KEY_ESCAPE)
        {
            self->cancelWork_();
            self->uiFocus_ = UiField::None;
            self->uiEdit_.clear();
            {
                std::lock_guard<std::mutex> lk(self->uiMsgMutex_);
                self->uiLastMsg_ = "Cancelled.";
            }
            self->updateWindowTitle_();
            return;
        }

        if (self->uiFocus_ != UiField::None)
        {
            if (key == GLFW_KEY_BACKSPACE) {
                if (!self->uiEdit_.empty()) self->uiEdit_.pop_back();
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                self->applyEdit_();
                self->uiFocus_ = UiField::None;
                self->uiEdit_.clear();
                self->updateWindowTitle_();
            }
            return;
        }

        if (key == GLFW_KEY_B) {
            self->requestBuild_(UiIndexToType_(self->uiTypeIndex_), self->uiSeed_);
            return;
        }
        if (key == GLFW_KEY_F) {
            self->requestFindPath_(self->uiStartX_, self->uiStartY_, self->uiEndX_, self->uiEndY_);
            return;
        }
    });

    glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int button, int action, int /*mods*/)
    {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

        double px = 0, py = 0;
        glfwGetCursorPos(w, &px, &py);

        int winW = 1, winH = 1;
        glfwGetWindowSize(w, &winW, &winH);
        if (winW <= 0) winW = 1;
        if (winH <= 0) winH = 1;

        const float mx = (float)((px / (double)winW) * 2.0 - 1.0);
        const float my = (float)(1.0 - (py / (double)winH) * 2.0);

        // ---- match ui.cpp layout
        const float panelX0 = -1.0f;

        const float sidePx = (self->fbW_ > 0 && self->fbH_ > 0) ? (float)std::min(self->fbW_, self->fbH_) : 0.0f;
        const float splitX = (self->fbW_ > 0) ? (1.0f - 2.0f * (sidePx / (float)self->fbW_)) : -0.25f;
        const float panelX1 = splitX;

        const float padX = 0.05f;
        const float padY = 0.05f;
        const float gap  = 0.02f;

        const float contentX0 = panelX0 + padX;
        const float contentX1 = panelX1 - padX;

        const float availW = std::max(0.01f, contentX1 - contentX0);
        float sq = (availW - gap * 3.0f) / 4.0f;
        sq = std::min(sq, 0.22f);
        sq = std::max(sq, 0.08f);

        const float yTop = 1.0f - padY;

        const float sizeY1 = yTop;
        const float sizeY0 = sizeY1 - sq;

        // Row 1: size buttons (also triggers build)
        for (int i = 0; i < 4; ++i)
        {
            const float x0 = contentX0 + i * (sq + gap);
            const float x1 = x0 + sq;

            if (Hit_(mx, my, x0, sizeY0, x1, sizeY1))
            {
                self->uiTypeIndex_ = i;
                self->requestBuild_(UiIndexToType_(i), self->uiSeed_);
                self->updateWindowTitle_();
                return;
            }
        }

        // Row 2: seed input
        const float seedY1 = sizeY0 - gap;
        const float seedH  = 0.14f;
        const float seedY0 = seedY1 - seedH;

        if (Hit_(mx, my, contentX0, seedY0, contentX1, seedY1))
        {
            self->uiFocus_ = UiField::Seed;
            self->uiEdit_ = std::to_string(self->uiSeed_);
            self->updateWindowTitle_();
            return;
        }

        // Row 3: start path button
        const float goY1 = seedY0 - gap;
        const float goH  = 0.14f;
        const float goY0 = goY1 - goH;

        if (Hit_(mx, my, contentX0, goY0, contentX1, goY1))
        {
            Maze snapshot{};
            {
                std::lock_guard<std::mutex> lk(self->latestMazeMutex_);
                if (!self->hasMaze_) {
                    std::lock_guard<std::mutex> lk2(self->uiMsgMutex_);
                    self->uiLastMsg_ = "No maze yet. Click a size to build first.";
                    return;
                }
                snapshot = self->latestMaze_;
            }

            const int H = (int)snapshot.grid.size();
            const int W = (H > 0) ? (int)snapshot.grid[0].size() : 0;
            if (W <= 2 || H <= 2) {
                std::lock_guard<std::mutex> lk2(self->uiMsgMutex_);
                self->uiLastMsg_ = "Maze too small.";
                return;
            }

            const int sx = 1, sy = 1;
            const int ex = W - 2, ey = H - 2;
            self->requestFindPath_(sx, sy, ex, ey);
            return;
        }
    });
}