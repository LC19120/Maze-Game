#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>
#include <sstream>

void Viewer::applyEdit()
{
    auto parseInt = [&](int &dst)
    {
        if (uiEdit.empty() || uiEdit == "-")
            return;
        try { dst = std::stoi(uiEdit); }
        catch (...) {}
    };

    switch (uiFocus)
    {
    case UI::Seed:        parseInt(uiSeed); break;

    case UI::BreakCount:
        parseInt(uiBreakCount);
        uiBreakCount = std::clamp(uiBreakCount, 0, 9); // <<< 1 digit
        break;

    case UI::StartX:      parseInt(uiStartX);      break;
    case UI::StartY:      parseInt(uiStartY);      break;
    case UI::EndX:        parseInt(uiEndX);        break;
    case UI::EndY:        parseInt(uiEndY);        break;
    case UI::UpdateEvery: parseInt(uiUpdateEvery); break;
    case UI::DelayMs:     parseInt(uiDelayMs);     break;
    default: break;
    }
}

void Viewer::initUiCallbacks()
{
    GLFWwindow *win = static_cast<GLFWwindow *>(window);
    glfwSetWindowUserPointer(win, this);

    glfwSetCharCallback(win, [](GLFWwindow *w, unsigned int codepoint)
    {
        auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (self->uiFocus == UI::None) return;

        if (codepoint > 127) return;
        const char ch = (char)codepoint;

        if (ch >= '0' && ch <= '9')
        {
            // Seed: at most 4 digits (rolling)
            if (self->uiFocus == UI::Seed)
            {
                if (self->uiEdit == "-") self->uiEdit.clear();

                if (self->uiEdit.size() < 4) self->uiEdit.push_back(ch);
                else { self->uiEdit.erase(self->uiEdit.begin()); self->uiEdit.push_back(ch); }
                return;
            }

            // BreakCount: exactly 1 digit (overwrite)
            if (self->uiFocus == UI::BreakCount)
            {
                self->uiEdit.clear();
                self->uiEdit.push_back(ch);
                return;
            }

            if (self->uiEdit.size() < 12) self->uiEdit.push_back(ch);
            return;
        }

        if (ch == '-')
        {
            // BreakCount 不允许负数
            if (self->uiFocus == UI::BreakCount) return;

            if (self->uiEdit.empty()) self->uiEdit.push_back(ch);
            return;
        }
    });

    glfwSetKeyCallback(win, [](GLFWwindow *w, int key, int /*scancode*/, int action, int /*mods*/)
                       {
        auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

        if (key == GLFW_KEY_ESCAPE)
        {
            self->uiFocus = UI::None;
            self->uiEdit.clear();
            self->updateWindowTitle();
            return;
        }

        if (self->uiFocus != UI::None)
        {
            if (key == GLFW_KEY_BACKSPACE) {
                if (!self->uiEdit.empty()) self->uiEdit.pop_back();
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                self->applyEdit();
                self->uiFocus = UI::None;
                self->uiEdit.clear();
                self->updateWindowTitle();
            }
            return;
        }

        if (key == GLFW_KEY_B) {
            self->buildMaze(self->uiSeed);
            return;
        }
        if (key == GLFW_KEY_F) {
    self->findPath(self->uiStartX, self->uiStartY, self->uiEndX, self->uiEndY, self->uiAlgoIndex);
    return;
} });

    glfwSetMouseButtonCallback(win, [](GLFWwindow *w, int button, int action, int /*mods*/)
    {
        auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
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

        const float sidePx = (self->fbW > 0 && self->fbH > 0) ? (float)std::min(self->fbW, self->fbH) : 0.0f;
        const float splitX = (self->fbW > 0) ? (1.0f - 2.0f * (sidePx / (float)self->fbW)) : -0.25f;
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

        if (Hit(mx, my, contentX0, buildY0, contentX1, buildY1))
        {
            self->uiFocus = UI::None;
            self->uiEdit.clear();
            self->buildMaze(self->uiSeed);
            self->updateWindowTitle();
            return;
        }

        // Row 2: SEED input (must match ui.cpp)
        const float seedH  = 0.14f;
        const float seedLabelPix = 0.0080f;
        const float seedLabelH   = 7.0f * seedLabelPix;

        const float seedLabelY1 = buildY0 - gap;
        const float seedLabelY0 = seedLabelY1 - seedLabelH;

        const float seedY1 = seedLabelY0 - 0.012f;
        const float seedY0 = seedY1 - seedH;

        if (Hit(mx, my, contentX0, seedY0, contentX1, seedY1))
        {
            self->uiFocus = UI::Seed;
            self->uiEdit = std::to_string(self->uiSeed);
            self->updateWindowTitle();
            return;
        }

        // +++ add: X/Y input boxes click (match ui.cpp)
        const float xyH = 0.12f;

        const float xyLabelY1 = seedY0 - gap;
        const float xyLabelY0 = xyLabelY1 - seedLabelH;

        const float xyY1 = xyLabelY0 - 0.012f;
        const float xyY0 = xyY1 - xyH;

        const float xyGapX = 0.03f;
        const float midX = (contentX0 + contentX1) * 0.5f;

        const float xBoxX0 = contentX0;
        const float xBoxX1 = midX - xyGapX * 0.5f;

        const float yBoxX0 = midX + xyGapX * 0.5f;
        const float yBoxX1 = contentX1;

        if (Hit(mx, my, xBoxX0, xyY0, xBoxX1, xyY1))
        {
            self->uiFocus = UI::StartX;
            self->uiEdit = std::to_string(self->uiStartX);
            self->updateWindowTitle();
            return;
        }

        if (Hit(mx, my, yBoxX0, xyY0, yBoxX1, xyY1))
        {
            self->uiFocus = UI::StartY;
            self->uiEdit = std::to_string(self->uiStartY);
            self->updateWindowTitle();
            return;
        }
        
        const float btnH = 0.11f;
        const float btnGap = 0.018f;
        const float bottomY0 = panelY0 + padY;

        // Row 0: PATH
        {
            const float y0 = bottomY0 + 0 * (btnH + btnGap);
            const float y1 = y0 + btnH;
            if (Hit(mx, my, contentX0, y0, contentX1, y1))
            {
                self->uiFocus = UI::None;
                self->uiEdit.clear();

                int W = 71, H = 71;
                if (self->mazeLoaded && !self->maze.grid.empty() && !self->maze.grid[0].empty())
                { H = (int)self->maze.grid.size(); W = (int)self->maze.grid[0].size(); }

                self->findPath(1, 1, std::max(1, W - 2), std::max(1, H - 2), 0);
                return;
            }
        }

        // Row 1: BREAK + [breakCount box]
        {
            const float y0 = bottomY0 + 1 * (btnH + btnGap);
            const float y1 = y0 + btnH;

            const float boxW = btnH;          // square (keep)
            const float boxGap = 0.012f;
            const float boxX0 = contentX1 - boxW;
            const float boxX1 = contentX1;
            const float btnX0 = contentX0;
            const float btnX1 = boxX0 - boxGap;

            // click on breakCount input box
            if (Hit(mx, my, boxX0, y0, boxX1, y1))
            {
                self->uiFocus = UI::BreakCount;
                self->uiEdit = std::to_string(std::clamp(self->uiBreakCount, 0, 9)); // <<< 1 digit
                self->updateWindowTitle();
                return;
            }

            // click on BREAK button
            if (Hit(mx, my, btnX0, y0, btnX1, y1))
            {
                self->uiFocus = UI::None;
                self->uiEdit.clear();

                int W = 71, H = 71;
                if (self->mazeLoaded && !self->maze.grid.empty() && !self->maze.grid[0].empty())
                { H = (int)self->maze.grid.size(); W = (int)self->maze.grid[0].size(); }

                self->findPath(1, 1, std::max(1, W - 2), std::max(1, H - 2), 1);
                return;
            }
        }

        // Row 2: COUNT (PathCounter)
        {
            const float y0 = bottomY0 + 2 * (btnH + btnGap);
            const float y1 = y0 + btnH;

            if (Hit(mx, my, contentX0, y0, contentX1, y1))
            {
                self->uiFocus = UI::None;
                self->uiEdit.clear();

                int W = 71, H = 71;
                if (self->mazeLoaded && !self->maze.grid.empty() && !self->maze.grid[0].empty())
                { H = (int)self->maze.grid.size(); W = (int)self->maze.grid[0].size(); }

                self->findPath(1, 1, std::max(1, W - 2), std::max(1, H - 2), 2);
                return;
            }
        }

        //Row 3: PASS (PathPasser)
        {
            const float y0 = bottomY0 + 3 * (btnH + btnGap);
            const float y1 = y0 + btnH;

            if (Hit(mx, my, contentX0, y0, contentX1, y1))
            {
                self->uiFocus = UI::None;
                self->uiEdit.clear();

                // mark last clicked algo as PASS for the result box
                self->uiAlgoIndex = 3;

                // pass through the entered X/Y (uiStartX/uiStartY are your X/Y inputs)
                self->passPath((uint32_t)self->uiStartX, (uint32_t)self->uiStartY);
                return;
            }
        }
    });
}