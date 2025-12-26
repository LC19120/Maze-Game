#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <cmath>
#include <cstdlib> // +++ add: strtol

// +++ add: C++11 compatible clamp
static float Clamp_(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
// --- add


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

// +++ add: tighter number renderer for narrow columns (visited etc.)
static void PushInt7Tight_(std::vector<Vertex>& out, int v,
                           float x, float y, float w, float h,
                           float r, float g, float b)
{
    std::string s = std::to_string(v);
    const size_t n = std::max<size_t>(s.size(), 1);

    // smaller gap so 5~6 digits still fit
    float gap = w * 0.055f;
    gap = std::min(gap, w * 0.10f);
    gap = std::max(gap, w * 0.02f);

    const float denom = (float)n + (float)(n - 1) * (gap / std::max(w, 1e-6f));
    const float cw = (denom > 0.0f) ? (w / denom) : w;

    float cx = x;
    for (char ch : s) {
        PushDigit7_(out, ch, cx, y, cw, h, r, g, b);
        cx += cw + gap;
    }
}
// --- add

namespace
{
    static std::array<uint8_t, 7> Glyph5x7_(char c)
    {
        // +++ support a-z by mapping to A-Z
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        // --- support

        switch (c)
        {
        case ' ': return {0,0,0,0,0,0,0};

        case 'A': return {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
        case 'B': return {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
        case 'C': return {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110};
        case 'D': return {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
        case 'E': return {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
        case 'F': return {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000};
        case 'G': return {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110};
        case 'H': return {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
        case 'I': return {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111};
        case 'J': return {0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100};
        case 'K': return {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
        case 'L': return {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
        case 'M': return {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001};
        case 'N': return {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
        case 'O': return {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
        case 'P': return {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000};
        case 'Q': return {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
        case 'R': return {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
        case 'S': return {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110};
        case 'T': return {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
        case 'U': return {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
        case 'V': return {0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100};
        case 'W': return {0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010};
        case 'X': return {0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001};
        case 'Y': return {0b10001,0b01010,0b00100,0b00100,0b00100,0b00100,0b00100};
        case 'Z': return {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111};

        case '*': return {0b00100,0b10101,0b01110,0b11111,0b01110,0b10101,0b00100};
        default:  return {0,0,0,0,0,0,0};
        }
    }

    static void PushText5x7_(std::vector<Vertex>& out,
                            std::string_view text,
                            float x, float y,
                            float pixelW, float pixelH,
                            float r, float g, float b)
    {
        float cx = x;
        for (char ch : text)
        {
            const auto g7 = Glyph5x7_(ch);
            for (int row = 0; row < 7; ++row)
            {
                for (int col = 0; col < 5; ++col)
                {
                    const bool on = (g7[row] & (1u << (4 - col))) != 0;
                    if (!on) continue;

                    const float x0 = cx + col * pixelW;
                    const float y0 = y  + (6 - row) * pixelH;
                    const float x1 = x0 + pixelW;
                    const float y1 = y0 + pixelH;
                    PushRect_(out, x0, y0, x1, y1, r, g, b);
                }
            }
            cx += 6.0f * pixelW; // 5 cols + 1 spacing
        }
    }

    static float TextWidth5x7_(std::string_view text, float pixelW)
    {
        return (float)text.size() * 6.0f * pixelW;
    }
}

void MazeViewer::renderUi_()
{
    std::vector<Vertex> ui;
    ui.reserve(4000);

    const float panelX0 = -1.0f;

    const float sidePx = (fbW_ > 0 && fbH_ > 0) ? (float)std::min(fbW_, fbH_) : 0.0f;
    const float splitX = (fbW_ > 0) ? (1.0f - 2.0f * (sidePx / (float)fbW_)) : -0.25f;
    const float panelX1 = splitX;

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

    // ---- Row 1: BUILD button (Medium only)
    const float availW = std::max(0.01f, contentX1 - contentX0);
    float buildH = std::min(0.22f, std::max(0.10f, availW * 0.22f));

    const float yTop = panelY1 - padY;
    const float buildY1 = yTop;
    const float buildY0 = buildY1 - buildH;

    // button bg
    PushRect_(ui, contentX0, buildY0, contentX1, buildY1, 0.75f, 0.75f, 0.75f);

    // label
    {
        const std::string_view label = "BUILD";
        const float pix = 0.0105f;
        const float tw  = TextWidth5x7_(label, pix);
        const float tx  = (contentX0 + contentX1) * 0.5f - tw * 0.5f;
        const float ty  = (buildY0 + buildY1) * 0.5f - (7.0f * pix) * 0.5f;
        PushText5x7_(ui, label, tx, ty, pix, pix, 0.08f, 0.08f, 0.08f);
    }

    // ---- Row 2: SEED label + seed input (was based on sizeY0; now based on buildY0)
    const float seedH  = 0.14f;

    const float seedLabelPix = 0.0080f;
    const float seedLabelH   = 7.0f * seedLabelPix;

    const float seedLabelY1 = buildY0 - gap;
    const float seedLabelY0 = seedLabelY1 - seedLabelH;

    const float seedY1 = seedLabelY0 - 0.012f;
    const float seedY0 = seedY1 - seedH;

    // draw "SEED" label (left aligned)
    PushText5x7_(ui, "SEED",
                 contentX0, seedLabelY0,
                 seedLabelPix, seedLabelPix,
                 0.92f, 0.92f, 0.92f);

    // seed input box + digits
    drawBox(contentX0, seedY0, contentX1, seedY1, uiFocus_ == UiField::Seed);

    int seedShown = uiSeed_;
    if (uiFocus_ == UiField::Seed && !uiEdit_.empty())
    {
        char* end = nullptr;
        const long v = std::strtol(uiEdit_.c_str(), &end, 10);
        if (end != uiEdit_.c_str()) // parsed at least one digit
            seedShown = (int)v;
    }

    PushInt7_(ui, seedShown,
              contentX0 + 0.02f, seedY0 + 0.02f,
              (contentX1 - contentX0) - 0.04f, (seedY1 - seedY0) - 0.04f,
              0.92f, 0.92f, 0.92f);

    // ---- Bottom: 6 algorithm buttons
    struct AlgoBtn { const char* label; float r,g,b; };

    const AlgoBtn algos[6] = {
        {"DFS",      0.20f, 0.70f, 0.25f},
        {"BFS",      0.20f, 0.55f, 0.95f},
        {"DIJKSTRA", 0.85f, 0.65f, 0.20f},
        {"A*",       0.70f, 0.35f, 0.90f},
        {"FLOYD",    0.20f, 0.75f, 0.75f},
        {"ALL",      0.65f, 0.65f, 0.65f},
    };

    const float btnH = 0.11f;
    const float btnGap = 0.018f;
    const float bottomY0 = panelY0 + padY;
    const float algosTop = bottomY0 + 6.0f * btnH + 5.0f * btnGap;

    // +++ DIV: algo min path len area between seed and algo buttons
    const float statsY0 = algosTop + gap;
    const float statsY1 = seedY0 - gap;

    if (statsY1 > statsY0 + 0.06f)
    {
        PushRect_(ui, contentX0, statsY0, contentX1, statsY1, 0.16f, 0.16f, 0.16f);

        // +++ add: row labels for the stats table
        const char* rows[5] = {"DFS", "BFS", "DIJKSTRA", "A*", "FLOYD"};
        // --- add

        std::array<int, 5> lens{};
        std::array<int, 5> vis{};
        std::array<int, 5> foundAt{};
        {
            std::lock_guard<std::mutex> lk(algoLenMutex_);
            lens = algoPathLen_;
            vis  = algoVisited_;
            foundAt = algoFoundAt_;
        }

        // rank: smaller foundAt => earlier arrival => better
        std::array<int, 5> rank{};
        std::array<int, 5> order{{0,1,2,3,4}};

        std::sort(order.begin(), order.end(), [&](int a, int b){
            const int fa = foundAt[(size_t)a];
            const int fb = foundAt[(size_t)b];

            const bool ha = (fa >= 0);
            const bool hb = (fb >= 0);
            if (ha != hb) return ha;                 // found ones first
            if (ha && hb && fa != fb) return fa < fb; // earlier hit first

            // tie-breakers (stable & deterministic):
            const int la = lens[(size_t)a], lb = lens[(size_t)b];
            if (la >= 0 && lb >= 0 && la != lb) return la < lb;
            const int va = vis[(size_t)a], vb = vis[(size_t)b];
            if (va != vb) return va < vb;
            return a < b;
        });

        // strict 1..5 (no gaps)
        for (int pos = 0; pos < 5; ++pos)
            rank[(size_t)order[(size_t)pos]] = pos + 1;

        const float rowH = (statsY1 - statsY0) / 5.0f;

        // +++ new: adaptive column widths (visited widest)
        const float availW = std::max(0.001f, contentX1 - contentX0);
        float numericTotalW = availW * 0.54f;                 // numbers take ~54% of panel width
        numericTotalW = Clamp_(numericTotalW, 0.28f, availW - 0.16f);

        float colRankW = numericTotalW * 0.18f;
        float colLenW  = numericTotalW * 0.36f;
        float colVisW  = numericTotalW - colRankW - colLenW;

        const float numX0 = contentX1 - numericTotalW;
        const float labelX0 = contentX0 + 0.01f;
        const float labelX1 = numX0 - 0.01f;
        const float labelW  = std::max(0.001f, labelX1 - labelX0);

        const float numPad = 0.012f;

        // +++ add: draw number cell with 60% height (reduce 40%), centered
        // +++ number box: shrink height and width, then center
        const float kNumHScale = 0.60f; // height = 60% (reduce 40%)
        const float kNumWScale = 0.80f; // width  = 80% (reduce 20%)

        auto numBox = [&](float x0, float y0, float w, float h,
                          float& outX, float& outY, float& outW, float& outH)
        {
            const float fullW = std::max(0.001f, w);
            const float fullH = std::max(0.001f, h);

            const float nw = std::max(0.001f, fullW * kNumWScale);
            const float nh = std::max(0.001f, fullH * kNumHScale);

            outW = nw;
            outH = nh;

            outX = x0 + (fullW - nw) * 0.5f; // center horizontally
            outY = y0 + (fullH - nh) * 0.5f; // center vertically
        };
        // --- number box

        for (int i = 0; i < 5; ++i)
        {
            const float y0 = statsY1 - (i + 1) * rowH;
            const float y1 = y0 + rowH;

            if (i == uiAlgoIndex_) {
                PushRect_(ui, contentX0 + 0.005f, y0 + 0.005f, contentX1 - 0.005f, y1 - 0.005f, 0.22f, 0.22f, 0.22f);
            }

            // --- label: auto shrink to fit labelW
            const std::string_view rowLabel = rows[i];
            float pix = (rowLabel == "DIJKSTRA") ? 0.0062f : 0.0078f;
            const float needW = TextWidth5x7_(rowLabel, pix);
            if (needW > labelW) {
                pix = std::max(0.0042f, labelW / (6.0f * (float)rowLabel.size()));
            }
            const float ly = (y0 + y1) * 0.5f - (7.0f * pix) * 0.5f;
            PushText5x7_(ui, rowLabel, labelX0, ly, pix, pix, 0.92f, 0.92f, 0.92f);

            // --- numeric columns (no per-row headers; saves space)
            const float rxRank0 = numX0;
            const float rxLen0  = rxRank0 + colRankW;
            const float rxVis0  = rxLen0  + colLenW;

            const float vy0 = y0 + 0.01f;
            const float vy1 = y1 - 0.01f;

            // rank
            {
                float bx, by, bw, bh;
                numBox(rxRank0 + numPad, vy0 + numPad,
                       colRankW - 2.0f*numPad, (vy1 - vy0) - 2.0f*numPad,
                       bx, by, bw, bh);

                if (rank[(size_t)i] >= 0)
                    PushInt7Tight_(ui, rank[(size_t)i], bx, by, bw, bh, 0.92f, 0.92f, 0.92f);
                else
                    PushDigit7_(ui, '-', bx, by, bw, bh, 0.92f, 0.92f, 0.92f);
            }

            // len
            {
                float bx, by, bw, bh;
                numBox(rxLen0 + numPad, vy0 + numPad,
                       colLenW - 2.0f*numPad, (vy1 - vy0) - 2.0f*numPad,
                       bx, by, bw, bh);

                if (lens[(size_t)i] >= 0)
                    PushInt7Tight_(ui, lens[(size_t)i], bx, by, bw, bh, 0.92f, 0.92f, 0.92f);
                else
                    PushDigit7_(ui, '-', bx, by, bw, bh, 0.92f, 0.92f, 0.92f);
            }

            // visited
            {
                float bx, by, bw, bh;
                numBox(rxVis0 + numPad, vy0 + numPad,
                       colVisW - 2.0f*numPad, (vy1 - vy0) - 2.0f*numPad,
                       bx, by, bw, bh);

                PushInt7Tight_(ui, std::max(0, vis[(size_t)i]), bx, by, bw, bh, 0.92f, 0.92f, 0.92f);
            }
        }
    }
    // --- DIV

    // ---- Bottom: 6 algorithm buttons (if you also want DIJKSTRA smaller there)
    for (int i = 0; i < 6; ++i)
    {
        const float y0 = bottomY0 + i * (btnH + btnGap);
        const float y1 = y0 + btnH;

        PushRect_(ui, contentX0, y0, contentX1, y1, algos[i].r, algos[i].g, algos[i].b);

        // label (DIJKSTRA smaller)
        const std::string_view label = algos[i].label;
        const float pix = (label == "DIJKSTRA") ? 0.0064f : 0.0105f; // was ~0.0072f

        const float tw  = TextWidth5x7_(label, pix);
        const float tx  = (contentX0 + contentX1) * 0.5f - tw * 0.5f;
        const float ty  = (y0 + y1) * 0.5f - (7.0f * pix) * 0.5f;

        PushText5x7_(ui, label, tx, ty, pix, pix, 0.08f, 0.08f, 0.08f);
    }

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