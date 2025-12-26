#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>
#include <sstream>

void MazeViewer::applyEdit_()
{
    auto parseInt = [&](int &dst)
    {
        if (uiEdit_.empty() || uiEdit_ == "-")
            return;
        try
        {
            dst = std::stoi(uiEdit_);
        }
        catch (...)
        {
        }
    };

    switch (uiFocus_)
    {
    case UiField::Seed:
        parseInt(uiSeed_);
        break;
    case UiField::StartX:
        parseInt(uiStartX_);
        break;
    case UiField::StartY:
        parseInt(uiStartY_);
        break;
    case UiField::EndX:
        parseInt(uiEndX_);
        break;
    case UiField::EndY:
        parseInt(uiEndY_);
        break;
    case UiField::UpdateEvery:
        parseInt(uiUpdateEvery_);
        break;
    case UiField::DelayMs:
        parseInt(uiDelayMs_);
        break;
    default:
        break;
    }
}

void MazeViewer::initUiCallbacks_()
{
    GLFWwindow *win = static_cast<GLFWwindow *>(window_);
    glfwSetWindowUserPointer(win, this);

    glfwSetCharCallback(win, [](GLFWwindow *w, unsigned int codepoint)
                        {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (self->uiFocus_ == UiField::None) return;

        if (codepoint > 127) return;
        const char ch = (char)codepoint;

        if (ch >= '0' && ch <= '9')
        {
            // Seed: keep at most 5 digits; if overflow, drop first digit then append new digit.
            if (self->uiFocus_ == UiField::Seed)
            {
                // treat "-" as empty for digit collection
                if (self->uiEdit_ == "-") self->uiEdit_.clear();

                if (self->uiEdit_.size() < 4)
                {
                    self->uiEdit_.push_back(ch);
                }
                else
                {
                    // remove first digit, append new digit (rolling)
                    self->uiEdit_.erase(self->uiEdit_.begin());
                    self->uiEdit_.push_back(ch);
                }
                return;
            }

            // Other fields: keep old behavior
            if (self->uiEdit_.size() < 12) self->uiEdit_.push_back(ch);
            return;
        }

        if (ch == '-')
        {
            if (self->uiEdit_.empty()) self->uiEdit_.push_back(ch);
            return;
        } });

    glfwSetKeyCallback(win, [](GLFWwindow *w, int key, int /*scancode*/, int action, int /*mods*/)
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
            self->requestBuild_(self->uiSeed_);
            return;
        }
        if (key == GLFW_KEY_F) {
    self->requestFindPath_(self->uiStartX_, self->uiStartY_, self->uiEndX_, self->uiEndY_, self->uiAlgoIndex_);
    return;
} });

    glfwSetMouseButtonCallback(win, [](GLFWwindow *w, int button, int action, int /*mods*/)
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

        int fbW = 1, fbH = 1;
        glfwGetFramebufferSize(w, &fbW, &fbH);
        if (fbW <= 0) fbW = 1;
        if (fbH <= 0) fbH = 1;

        // convert cursor pos (window coords) -> framebuffer coords
        const double sx = (double)fbW / (double)winW;
        const double sy = (double)fbH / (double)winH;
        const double fpx = px * sx;
        const double fpy = py * sy;

        // framebuffer coords -> NDC
        const float mx = (float)((fpx / (double)fbW) * 2.0 - 1.0);
        const float my = (float)(1.0 - (fpy / (double)fbH) * 2.0);

        // ---- match ui.cpp layout
        const float panelX0 = -1.0f;
        const float panelY0 = -1.0f;

        const float sidePx = (self->fbW_ > 0 && self->fbH_ > 0) ? (float)std::min(self->fbW_, self->fbH_) : 0.0f;
        const float splitX = (self->fbW_ > 0) ? (1.0f - 2.0f * (sidePx / (float)self->fbW_)) : -0.25f;
        const float panelX1 = splitX;

        const float padX = 0.05f;
        const float padY = 0.05f;
        const float gap  = 0.02f;

        const float contentX0 = panelX0 + padX;
        const float contentX1 = panelX1 - padX;

        // Row 1: BUILD button (must match ui.cpp)
        const float availW = std::max(0.01f, contentX1 - contentX0);
        float buildH = std::min(0.22f, std::max(0.10f, availW * 0.22f));

        const float yTop = 1.0f - padY;
        const float buildY1 = yTop;
        const float buildY0 = buildY1 - buildH;

        if (Hit_(mx, my, contentX0, buildY0, contentX1, buildY1))
        {
            self->uiFocus_ = UiField::None;
            self->uiEdit_.clear();

            self->requestBuild_(self->uiSeed_);
            self->updateWindowTitle_();
            return;
        }

        // Row 2: SEED label + seed input (must match ui.cpp)
        const float seedH  = 0.14f;
        const float seedLabelPix = 0.0080f;
        const float seedLabelH   = 7.0f * seedLabelPix;

        const float seedLabelY1 = buildY0 - gap;
        const float seedLabelY0 = seedLabelY1 - seedLabelH;

        const float seedY1 = seedLabelY0 - 0.012f;
        const float seedY0 = seedY1 - seedH;

        if (Hit_(mx, my, contentX0, seedY0, contentX1, seedY1))
        {
            self->uiFocus_ = UiField::Seed;
            self->uiEdit_ = std::to_string(self->uiSeed_);
            self->updateWindowTitle_();
            return;
        }

        // Bottom: 7 algorithm buttons (match ui.cpp)
        struct AlgoBtn { const char* label; };
        const AlgoBtn algos[7] = {
            {"DFS"}, {"BFS"}, {"BFS+"}, {"DIJKSTRA"}, {"A*"}, {"FLOYD"}, {"ALL"}
        };

        const float btnH = 0.11f;
        const float btnGap = 0.018f;
        const float bottomY0 = panelY0 + padY;

        for (int i = 0; i < 7; ++i) // was 6
        {
            const float y0 = bottomY0 + i * (btnH + btnGap);
            const float y1 = y0 + btnH;

            if (!Hit_(mx, my, contentX0, y0, contentX1, y1))
                continue;

            self->uiFocus_ = UiField::None;
            self->uiEdit_.clear();

            Maze snapshot{};
            {
                std::lock_guard<std::mutex> lk(self->latestMazeMutex_);
                if (!self->hasMaze_) {
                    std::lock_guard<std::mutex> lk2(self->uiMsgMutex_);
                    self->uiLastMsg_ = "No maze yet. Click BUILD first.";
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

            {
                std::lock_guard<std::mutex> lk2(self->uiMsgMutex_);
                self->uiLastMsg_ = std::string("Algorithm: ") + algos[i].label;
            }

            const int sx = 1, sy = 1;
            const int ex = W - 2, ey = H - 2;

            // NOTE: PathFinder currently always uses DFSExploer inside.
            // This click now *responds*; next step is to pass algo into PathFinder.
            self->requestFindPath_(sx, sy, ex, ey, i);
            return;
        } });
}