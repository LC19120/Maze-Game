#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <array>
#include <string_view>
#include <cstdlib>

// 将一段“线段”以矩形的方式压入顶点数组（用于七段数码管的段绘制）
static void PushSeg(std::vector<Vertex>& out, float x0, float y0, float x1, float y1, float r, float g, float b)
{
    PushRect(out, x0, y0, x1, y1, r, g, b);
}

// 绘制单个七段数码管字符（0-9、'-'），并将对应矩形段压入顶点数组
static void PushDigit7(std::vector<Vertex>& out, char ch,
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

    auto segA = [&]{ PushSeg(out, ax0, ay0, ax1, ay1, r, g, b); };
    auto segB = [&]{ PushSeg(out, bx0, by0, bx1, by1, r, g, b); };
    auto segC = [&]{ PushSeg(out, cx0, cy0, cx1, cy1, r, g, b); };
    auto segD = [&]{ PushSeg(out, dx0, dy0, dx1, dy1, r, g, b); };
    auto segE = [&]{ PushSeg(out, ex0, ey0, ex1, ey1, r, g, b); };
    auto segF = [&]{ PushSeg(out, fx0, fy0, fx1, fy1, r, g, b); };
    auto segG = [&]{ PushSeg(out, gx0, gy0, gx1, gy1, r, g, b); };

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

// 将整数按七段数码管形式绘制（按位均分宽度+固定间距），并压入顶点数组
static void PushInt7(std::vector<Vertex>& out, int v,
                      float x, float y, float w, float h,
                      float r, float g, float b)
{
    std::string s = std::to_string(v);
    const float gap = w * 0.12f;
    const float cw = (w - gap * (float)(std::max<size_t>(s.size(), 1) - 1)) / (float)std::max<size_t>(s.size(), 1);
    float cx = x;
    for (char ch : s) {
        PushDigit7(out, ch, cx, y, cw, h, r, g, b);
        cx += cw + gap;
    }
}

// 更紧凑的整数七段数码管渲染：减少间距以适配窄列/多位数字
static void PushInt7Tight(std::vector<Vertex>& out, int v,
                           float x, float y, float w, float h,
                           float r, float g, float b)
{
    std::string s = std::to_string(v);
    const size_t n = std::max<size_t>(s.size(), 1);
    float gap = w * 0.055f;
    gap = std::min(gap, w * 0.10f);
    gap = std::max(gap, w * 0.02f);

    const float denom = (float)n + (float)(n - 1) * (gap / std::max(w, 1e-6f));
    const float cw = (denom > 0.0f) ? (w / denom) : w;

    float cx = x;
    for (char ch : s) {
        PushDigit7(out, ch, cx, y, cw, h, r, g, b);
        cx += cw + gap;
    }
}

namespace
{
    static std::array<uint8_t, 7> Glyph5x7(char c)
    {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        switch (c)
        {
        case ' ': return {0,0,0,0,0,0,0};
        case '-': return {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000};
        case '0': return {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110};
        case '1': return {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
        case '2': return {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111};
        case '3': return {0b01110,0b10001,0b00001,0b00110,0b00001,0b10001,0b01110};
        case '4': return {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010};
        case '5': return {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110};
        case '6': return {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110};
        case '7': return {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000};
        case '8': return {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110};
        case '9': return {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b11100};
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
        case '+': return {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000};
        default:  return {0,0,0,0,0,0,0};
        }
    }

    // 使用 5x7 点阵字体绘制文本：按像素大小生成小矩形并压入顶点数组
    static void PushText5x7(std::vector<Vertex>& out,
                            std::string_view text,
                            float x, float y,
                            float pixelW, float pixelH,
                            float r, float g, float b)
    {
        float cx = x;
        for (char ch : text)
        {
            const auto g7 = Glyph5x7(ch);
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
                    PushRect(out, x0, y0, x1, y1, r, g, b);
                }
            }
            cx += 6.0f * pixelW;
        }
    }

    // 计算 5x7 点阵文本在给定像素宽度下的总宽度（含字符间距）
    static float TextWidth5x7(std::string_view text, float pixelW)
    {
        return (float)text.size() * 6.0f * pixelW;
    }
}

// 在按钮矩形内居中绘制标签文本（使用 5x7 点阵字体）
static void DrawBtnLabel(std::vector<Vertex>& out,
                          std::string_view label,
                          float x0, float y0, float x1, float y1,
                          float pix = 0.0100f,
                          float r = 0.08f, float g = 0.08f, float b = 0.08f)
{
    const float tw = TextWidth5x7(label, pix);
    const float th = 7.0f * pix;
    const float tx = (x0 + x1) * 0.5f - tw * 0.5f;
    const float ty = (y0 + y1) * 0.5f - th * 0.5f;
    PushText5x7(out, label, tx, ty, pix, pix, r, g, b);
}

// 渲染左侧 UI 面板：按钮、输入框与结果展示，并上传顶点到 OpenGL 绘制
void Viewer::renderUi()
{
    std::vector<Vertex> ui;
    ui.reserve(4000);

    const float panelX0 = -1.0f;

    const float sidePx = (fbW > 0 && fbH > 0) ? (float)std::min(fbW, fbH) : 0.0f;
    const float splitX = (fbW > 0) ? (1.0f - 2.0f * (sidePx / (float)fbW)) : -0.25f;
    const float panelX1 = splitX;

    const float panelY0 = -1.0f, panelY1 = 1.0f;

    // panel bg
    PushRect(ui, panelX0, panelY0, panelX1, panelY1, 0.12f, 0.12f, 0.12f);

    auto drawBox = [&](float x0, float y0, float x1, float y1, bool focused)
    {
        const float br = focused ? 0.95f : 0.35f;
        const float bg = focused ? 0.85f : 0.35f;
        const float bb = focused ? 0.20f : 0.35f;
        PushRect(ui, x0 - 0.005f, y0 - 0.005f, x1 + 0.005f, y1 + 0.005f, br, bg, bb);
        PushRect(ui, x0, y0, x1, y1, 0.18f, 0.18f, 0.18f);
    };

    const float padX = 0.05f;
    const float padY = 0.05f;
    const float gap  = 0.02f;

    const float contentX0 = panelX0 + padX;
    const float contentX1 = panelX1 - padX;

    // ---- Row 1: BUILD button
    const float availW = std::max(0.01f, contentX1 - contentX0);
    float buildH = std::min(0.22f, std::max(0.10f, availW * 0.22f));

    const float yTop = panelY1 - padY;
    const float buildY1 = yTop;
    const float buildY0 = buildY1 - buildH;

    // button bg
    PushRect(ui, contentX0, buildY0, contentX1, buildY1, 0.75f, 0.75f, 0.75f);

    // label
    {
        const std::string_view label = "BUILD";
        const float pix = 0.0105f;
        const float tw  = TextWidth5x7(label, pix);
        const float tx  = (contentX0 + contentX1) * 0.5f - tw * 0.5f;
        const float ty  = (buildY0 + buildY1) * 0.5f - (7.0f * pix) * 0.5f;
        PushText5x7(ui, label, tx, ty, pix, pix, 0.08f, 0.08f, 0.08f);
    }

    // ---- Row 2: SEED label + seed input
    const float seedH  = 0.14f;

    const float seedLabelPix = 0.0080f;
    const float seedLabelH   = 7.0f * seedLabelPix;

    const float seedLabelY1 = buildY0 - gap;
    const float seedLabelY0 = seedLabelY1 - seedLabelH;

    const float seedY1 = seedLabelY0 - 0.012f;
    const float seedY0 = seedY1 - seedH;

    // draw "SEED" label (left aligned)
    PushText5x7(ui, "SEED",
                 contentX0, seedLabelY0,
                 seedLabelPix, seedLabelPix,
                 0.92f, 0.92f, 0.92f);

    // seed input box + digits
    drawBox(contentX0, seedY0, contentX1, seedY1, uiFocus == UI::Seed);

    int seedShown = uiSeed;
    if (uiFocus == UI::Seed && !uiEdit.empty())
    {
        char* end = nullptr;
        const long v = std::strtol(uiEdit.c_str(), &end, 10);
        if (end != uiEdit.c_str())
            seedShown = (int)v;
    }

    PushInt7(ui, seedShown,
             contentX0 + 0.02f, seedY0 + 0.02f,
             (contentX1 - contentX0) - 0.04f, (seedY1 - seedY0) - 0.04f,
             0.92f, 0.92f, 0.92f);

    // +++ add: X / Y input boxes under SEED (visual only, same style)
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

    // labels
    PushText5x7(ui, "X",
                 xBoxX0, xyLabelY0,
                 seedLabelPix, seedLabelPix,
                 0.92f, 0.92f, 0.92f);
    PushText5x7(ui, "Y",
                 yBoxX0, xyLabelY0,
                 seedLabelPix, seedLabelPix,
                 0.92f, 0.92f, 0.92f);

    // boxes (focus highlight supported)
    drawBox(xBoxX0, xyY0, xBoxX1, xyY1, uiFocus == UI::StartX);
    drawBox(yBoxX0, xyY0, yBoxX1, xyY1, uiFocus == UI::StartY);

    // numbers (preview like seed when focused)
    int xShown = uiStartX;
    int yShown = uiStartY;

    if (uiFocus == UI::StartX && !uiEdit.empty())
    {
        char* end = nullptr;
        const long v = std::strtol(uiEdit.c_str(), &end, 10);
        if (end != uiEdit.c_str())
            xShown = (int)v;
    }
    if (uiFocus == UI::StartY && !uiEdit.empty())
    {
        char* end = nullptr;
        const long v = std::strtol(uiEdit.c_str(), &end, 10);
        if (end != uiEdit.c_str())
            yShown = (int)v;
    }

    PushInt7Tight(ui, xShown,
                  xBoxX0 + 0.02f, xyY0 + 0.02f,
                  (xBoxX1 - xBoxX0) - 0.04f, (xyY1 - xyY0) - 0.04f,
                  0.92f, 0.92f, 0.92f);

    PushInt7Tight(ui, yShown,
                  yBoxX0 + 0.02f, xyY0 + 0.02f,
                  (yBoxX1 - yBoxX0) - 0.04f, (xyY1 - xyY0) - 0.04f,
                  0.92f, 0.92f, 0.92f);
    // --- add

    // --- remove: PathPasser button UNDER X/Y
    // (delete the whole old PASS button block here)

    // +++ add: Result box UNDER X/Y (shows PATH/BREAK length or COUNT ways)
    {
        const float resH  = 0.11f;
        const float resY1 = xyY0 - gap;
        const float resY0 = resY1 - resH;

        // box style (not focusable)
        drawBox(contentX0, resY0, contentX1, resY1, false);

        // decide what to show based on last clicked algo (uiAlgoIndex)
        std::string_view tag = "LEN";
        int value = 0;

        if (uiAlgoIndex == 2) // COUNT
        {
            tag = "WAYS";
            value = (anim.totalPaths > 0) ? anim.totalPaths : std::max(0, lastCountWays);
        }
        else if (uiAlgoIndex == 1) // BREAK
        {
            tag = "LEN";
            value = std::max(0, lastBreakLen);
        }
        else if (uiAlgoIndex == 3) // +++ PASS
        {
            tag = "LEN";
            value = std::max(0, lastPassLen);
        }
        else // PATH (A*)
        {
            tag = "LEN";
            value = std::max(0, lastPathLen);
        }

        // left label
        const float pix = 0.0085f;
        PushText5x7(ui, tag,
                     contentX0 + 0.018f, resY0 + 0.030f,
                     pix, pix,
                     0.92f, 0.92f, 0.92f);

        // right number
        PushInt7Tight(ui, value,
                      contentX1 - 0.33f, resY0 + 0.018f,
                      0.31f, (resY1 - resY0) - 0.036f,
                      0.92f, 0.92f, 0.92f);
    }
    // --- add

    // ---- Bottom: PATH / BREAK[...] / COUNT
    const float btnH = 0.11f;
    const float btnGap = 0.018f;
    const float bottomY0 = panelY0 + padY;

    // Row 0: PATH
    {
        const float y0 = bottomY0 + 0 * (btnH + btnGap);
        const float y1 = y0 + btnH;
        PushRect(ui, contentX0, y0, contentX1, y1, 0.20f, 0.55f, 1.00f);
        DrawBtnLabel(ui, "PATH", contentX0, y0, contentX1, y1);
    }

    // Row 1: BREAK + [breakCount box]
    {
        const float y0 = bottomY0 + 1 * (btnH + btnGap);
        const float y1 = y0 + btnH;

        const float boxW = btnH;     // square
        const float boxGap = 0.012f;

        const float boxX0 = contentX1 - boxW;
        const float boxX1 = contentX1;
        const float btnX0 = contentX0;
        const float btnX1 = boxX0 - boxGap;

        // break button (yellow)
        PushRect(ui, btnX0, y0, btnX1, y1, 1.00f, 0.92f, 0.10f);
        DrawBtnLabel(ui, "BREAK", btnX0, y0, btnX1, y1);

        // breakCount input box
        drawBox(boxX0, y0, boxX1, y1, uiFocus == UI::BreakCount);

        // preview like Seed, but clamp to one digit (0..9)
        int shown = std::clamp(uiBreakCount, 0, 9);
        if (uiFocus == UI::BreakCount && !uiEdit.empty())
        {
            char* end = nullptr;
            const long v = std::strtol(uiEdit.c_str(), &end, 10);
            if (end != uiEdit.c_str())
                shown = (int)std::clamp<long>(v, 0, 9);
        }

        PushInt7Tight(ui, shown,
                      boxX0 + 0.012f, y0 + 0.020f,
                      (boxX1 - boxX0) - 0.024f, (y1 - y0) - 0.040f,
                      0.92f, 0.92f, 0.92f);
    }

    // Row 2: COUNT (violet)
    {
        const float y0 = bottomY0 + 2 * (btnH + btnGap);
        const float y1 = y0 + btnH;

        PushRect(ui, contentX0, y0, contentX1, y1, 0.65f, 0.25f, 0.95f);
        DrawBtnLabel(ui, "COUNT", contentX0, y0, contentX1, y1);
    }

    {
        const float y0 = bottomY0 + 3 * (btnH + btnGap);
        const float y1 = y0 + btnH;

        PushRect(ui, contentX0, y0, contentX1, y1, 0.20f, 0.85f, 0.75f);
        DrawBtnLabel(ui, "PASS", contentX0, y0, contentX1, y1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui.size() * sizeof(Vertex)), ui.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    uiVertexCount = (int)ui.size();
    if (uiVertexCount > 0)
    {
        glUseProgram(program);
        glBindVertexArray(uiVao);
        glDrawArrays(GL_TRIANGLES, 0, uiVertexCount);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}