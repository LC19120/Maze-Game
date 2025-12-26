#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>

void MazeViewer::rebuildMeshFromMaze_(const Maze& m)
{
    const auto& grid = m.grid;
    const int rows = (int)grid.size();
    if (rows <= 0) { vertexCount_ = 0; return; }
    const int cols = (int)grid[0].size();
    if (cols <= 0) { vertexCount_ = 0; return; }

    std::vector<Vertex> verts;
    verts.reserve((size_t)rows * (size_t)cols * 6);

    // viewport is square, so build mesh in a square NDC [-1,1]
    const float gridW = 2.0f;
    const float gridH = 2.0f;

    const float cellW = gridW / (float)cols;
    const float cellH = gridH / (float)rows;
    const float cell  = std::min(cellW, cellH);

    // center it in the square
    const float totalW = cell * cols;
    const float totalH = cell * rows;
    const float startX = -totalW * 0.5f;
    const float startY =  totalH * 0.5f;

    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;

    // match button colors:
    const float dfsR = 0.20f, dfsG = 0.70f, dfsB = 0.25f; // 2
    const float bfsR = 0.20f, bfsG = 0.55f, bfsB = 0.95f; // 3
    const float dijR = 0.85f, dijG = 0.65f, dijB = 0.20f; // 4
    const float astR = 0.70f, astG = 0.35f, astB = 0.90f; // 5
    const float floR = 0.20f, floG = 0.75f, floB = 0.75f; // 6

    const float visitedA = 0.38f; // +++ 半透明访问格
    const float opaqueA  = 1.00f;

    // +++ visited colors (lower saturation)
    auto desat = [&](float r, float g, float b) {
        // mix towards gray to reduce saturation
        const float gray = (r + g + b) / 3.0f;
        const float t = 0.55f; // 0..1, bigger => more gray
        return std::array<float,3>{
            r * (1.0f - t) + gray * t,
            g * (1.0f - t) + gray * t,
            b * (1.0f - t) + gray * t
        };
    };
    const auto vdfs = desat(dfsR, dfsG, dfsB); // 12
    const auto vbfs = desat(bfsR, bfsG, bfsB); // 13
    const auto vdij = desat(dijR, dijG, dijB); // 14
    const auto vast = desat(astR, astG, astB); // 15
    const auto vflo = desat(floR, floG, floB); // 16
    // --- visited colors

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + c * cell;
            const float y0 = startY - (r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = grid[r][c];
            float rr = pathR, gg = pathG, bb = pathB, aa = opaqueA;

            if (v == 1) { rr = wallR; gg = wallG; bb = wallB; aa = opaqueA; }
            else if (v == 2) { rr = dfsR; gg = dfsG; bb = dfsB; aa = opaqueA; }
            else if (v == 3) { rr = bfsR; gg = bfsG; bb = bfsB; aa = opaqueA; }
            else if (v == 4) { rr = dijR; gg = dijG; bb = dijB; aa = opaqueA; }
            else if (v == 5) { rr = astR; gg = astG; bb = astB; aa = opaqueA; }
            else if (v == 6) { rr = floR; gg = floG; bb = floB; aa = opaqueA; }

            // visited (12..16): same color, but translucent
            else if (v == 12) { rr = dfsR; gg = dfsG; bb = dfsB; aa = visitedA; }
            else if (v == 13) { rr = bfsR; gg = bfsG; bb = bfsB; aa = visitedA; }
            else if (v == 14) { rr = dijR; gg = dijG; bb = dijB; aa = visitedA; }
            else if (v == 15) { rr = astR; gg = astG; bb = astB; aa = visitedA; }
            else if (v == 16) { rr = floR; gg = floG; bb = floB; aa = visitedA; }

            PushRect_(verts, x0, y0, x1, y1, rr, gg, bb, aa);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertexCount_ = (int)verts.size();
}

void MazeViewer::rebuildMeshIfDirty_()
{
    Maze local{};
    bool doRebuild = false;

    {
        std::lock_guard<std::mutex> lock(mazeMutex_);
        if (mazeLoaded_ && mazeDirty_) {
            local = maze_;
            mazeDirty_ = false;
            doRebuild = true;
        }
    }

    if (doRebuild) rebuildMeshFromMaze_(local);
}

void MazeViewer::drawMaze_()
{
    rebuildMeshIfDirty_();

    if (vertexCount_ > 0)
    {
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}