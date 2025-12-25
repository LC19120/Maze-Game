#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>

static void PushSeg_(std::vector<Vertex>& out, float x0, float y0, float x1, float y1, float r, float g, float b)
{
    PushRect_(out, x0, y0, x1, y1, r, g, b);
}

static void PushDigit7_(std::vector<Vertex>& out, char ch,
                        float x, float y, float w, float h,
                        float r, float g, float b)
{
    const float t = std::min(w, h) * 0.18f;
    const float x0 = x, x1 = x + w;
    const float y0 = y, y1 = y + h;

    const float ax0 = x0 + t, ax1 = x1 - t, ay0 = y1 - t, ay1 = y1;
    const float dx0 = x0 + t, dx1 = x1 - t, dy0 = y0,     dy1 = y0 + t;
    const float gx0 = x0 + t, gx1 = x1 - t, gy0 = y0 + (h - t) * 0.5f, gy1 = gy0 + t;

    const float fx0 = x0,     fx1 = x0 + t, fy0 = y0 + (h * 0.5f), fy1 = y1 - t;
    const float ex0 = x0,     ex1 = x0 + t, ey0 = y0 + t,          ey1 = y0 + (h * 0.5f);

    const float bx0 = x1 - t, bx1 = x1,     by0 = y0 + (h * 0.5f), by1 = y1 - t;
    const float cx0 = x1 - t, cx1 = x1,     cy0 = y0 + t,          cy1 = y0 + (h * 0.5f);

    auto segA = [&]{ PushSeg_(out, ax0, ay0, ax1, ay1, r, g, b); };
    auto segB = [&]{ PushSeg_(out, bx0, by0, bx1, by1, r, g, b); };
    auto segC = [&]{ PushSeg_(out, cx0, cy0, cx1, cy1, r, g, b); };
    auto segD = [&]{ PushSeg_(out, dx0, dy0, dx1, dy1, r, g, b); };
    auto segE = [&]{ PushSeg_(out, ex0, ey0, ex1, ey1, r, g, b); };
    auto segF = [&]{ PushSeg_(out, fx0, fy0, fx1, fy1, r, g, b); };
    auto segG = [&]{ PushSeg_(out, gx0, gy0, gx1, gy1, r, g, b); };

    auto on = [&](bool A,bool B,bool C,bool D,bool E,bool F,bool G){
        if (A) segA(); if (B) segB(); if (C) segC(); if (D) segD();
        if (E) segE(); if (F) segF(); if (G) segG();
    };

    switch (ch) {
        case '0': on(true,true,true,true,true,true,false); break;
        case '1': on(false,true,true,false,false,false,false); break;
        case '2': on(true,true,false,true,true,false,true); break;
        case '3': on(true,true,true,true,false,false,true); break;
        case '4': on(false,true,true,false,false,true,true); break;
        case '5': on(true,false,true,true,false,true,true); break;
        case '6': on(true,false,true,true,true,true,true); break;
        case '7': on(true,true,true,false,false,false,false); break;
        case '8': on(true,true,true,true,true,true,true); break;
        case '9': on(true,true,true,true,false,true,true); break;
        case '-': on(false,false,false,false,false,false,true); break;
        default: break;
    }
}

static void PushInt7_(std::vector<Vertex>& out, int v,
                      float x, float y, float w, float h,
                      float r, float g, float b)
{
    std::string s = std::to_string(v);
    const float gap = w * 0.12f;
    const float cw = (w - gap * (float)(std::max<size_t>(s.size(), 1) - 1)) / (float)std::max<size_t>(s.size(), 1);
    float cx = x;
    for (char ch : s) {
        PushDigit7_(out, ch, cx, y, cw, h, r, g, b);
        cx += cw + gap;
    }
}

void MazeViewer::renderUi_()
{
    std::vector<Vertex> ui;
    ui.reserve(2000);

    const float panelX0 = -1.0f;

    const float sidePx = (fbW_ > 0 && fbH_ > 0) ? (float)std::min(fbW_, fbH_) : 0.0f;
    const float splitX = (fbW_ > 0) ? (1.0f - 2.0f * (sidePx / (float)fbW_)) : -0.25f;
    const float panelX1 = splitX; // UI ends where maze square begins

    const float panelY0 = -1.0f, panelY1 = 1.0f;

    // panel bg
    PushRect_(ui, panelX0, panelY0, panelX1, panelY1, 0.12f, 0.12f, 0.12f);

    auto drawBox = [&](float x0, float y0, float x1, float y1, bool focused)
    {
        const float br = focused ? 0.95f : 0.35f;
        const float bg = focused ? 0.85f : 0.35f;
        const float bb = focused ? 0.20f : 0.35f;
        PushRect_(ui, x0 - 0.005f, y0 - 0.005f, x1 + 0.005f, y1 + 0.005f, br, bg, bb);
        PushRect_(ui, x0, y0, x1, y1, 0.18f, 0.18f, 0.18f);
    };

    const float padX = 0.05f;
    const float padY = 0.05f;
    const float gap  = 0.02f;

    const float contentX0 = panelX0 + padX;
    const float contentX1 = panelX1 - padX;

    // ---- Row 1: 4 square size buttons
    const float availW = std::max(0.01f, contentX1 - contentX0);
    float sq = (availW - gap * 3.0f) / 4.0f;
    sq = std::min(sq, 0.22f); // keep within panel height
    sq = std::max(sq, 0.08f);

    const float yTop = panelY1 - padY;
    const float sizeY1 = yTop;
    const float sizeY0 = sizeY1 - sq;

    for (int i = 0; i < 4; ++i)
    {
        const float x0 = contentX0 + i * (sq + gap);
        const float x1 = x0 + sq;

        const bool sel = (uiTypeIndex_ == i);
        PushRect_(ui, x0, sizeY0, x1, sizeY1,
                  sel ? 0.85f : 0.30f,
                  sel ? 0.70f : 0.30f,
                  sel ? 0.20f : 0.30f);

        // draw "1..4" as the label (7-seg digits)
        PushDigit7_(ui, char('1' + i),
                    x0 + 0.02f, sizeY0 + 0.02f,
                    (x1 - x0) - 0.04f, (sizeY1 - sizeY0) - 0.04f,
                    0.05f, 0.05f, 0.05f);
    }

    // ---- Row 2: seed input (one wide box)
    const float seedY1 = sizeY0 - gap;
    const float seedH  = 0.14f;
    const float seedY0 = seedY1 - seedH;

    drawBox(contentX0, seedY0, contentX1, seedY1, uiFocus_ == UiField::Seed);

    const int seedShown =
        (uiFocus_ == UiField::Seed && !uiEdit_.empty())
            ? ([](const std::string& s){ try { return std::stoi(s); } catch (...) { return 0; } }(uiEdit_))
            : uiSeed_;

    PushInt7_(ui, seedShown,
              contentX0 + 0.02f, seedY0 + 0.02f,
              (contentX1 - contentX0) - 0.04f, (seedY1 - seedY0) - 0.04f,
              0.92f, 0.92f, 0.92f);

    // ---- Row 3: start path button
    const float goY1 = seedY0 - gap;
    const float goH  = 0.14f;
    const float goY0 = goY1 - goH;

    PushRect_(ui, contentX0, goY0, contentX1, goY1, 0.20f, 0.55f, 0.95f); // blue button
    // (optional) no text; if you want a label, we can add a simple 7-seg "GO" substitute later.

    // upload + draw
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui.size() * sizeof(Vertex)), ui.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    uiVertexCount_ = (int)ui.size();

    if (uiVertexCount_ > 0)
    {
        glUseProgram(program_);
        glBindVertexArray(uiVao_);
        glDrawArrays(GL_TRIANGLES, 0, uiVertexCount_);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}