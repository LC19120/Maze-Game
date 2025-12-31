#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>

// +++ add: missing definition (fix linker error)
void Viewer::rebuildMeshIfDirty()
{
    Maze local{};
    bool doRebuild = false;

        const bool fbChanged = (meshBuiltFbW != fbW) || (meshBuiltFbH != fbH);

        if (mazeLoaded && (mazeDirty || fbChanged))
        {
            local = maze;
            mazeDirty = false;
            doRebuild = true;

            meshBuiltFbW = fbW;
            meshBuiltFbH = fbH;
        }
    

    if (doRebuild)
        rebuildMeshFromMaze(local);
}
// --- add

void Viewer::rebuildMeshFromMaze(const Maze& m)
{
    const auto& grid = m.grid;
    const int rows = (int)grid.size();
    if (rows <= 0) { vertexCount = 0; return; }
    const int cols = (int)grid[0].size();
    if (cols <= 0) { vertexCount = 0; return; }

    std::vector<Vertex> verts;
    verts.reserve((size_t)rows * (size_t)cols * 6);

    const float cell = 2.0f / (float)std::max(rows, cols);

    const float totalW = cell * (float)cols;
    const float totalH = cell * (float)rows;

    const float startX = -1.0f + (2.0f - totalW) * 0.5f;
    const float startY =  1.0f - (2.0f - totalH) * 0.5f;

    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;

    const float dfsR = 0.95f, dfsG = 0.20f, dfsB = 0.20f; // 2
    const float bfsR = 1.00f, bfsG = 0.55f, bfsB = 0.05f; // 3
    const float bfs2R= 1.00f, bfs2G= 0.92f, bfs2B= 0.10f; // 7
    const float dijR = 0.20f, dijG = 0.85f, dijB = 0.25f; // 4
    const float astR = 0.20f, astG = 0.55f, astB = 1.00f; // 5
    const float floR = 0.65f, floG = 0.25f, floB = 0.95f; // 6

    const float visitedA = 0.50f;
    const float opaqueA  = 1.00f;

    // +++ add: PASS color (same as PASS button / XY marker)
    const float passR = 0.20f, passG = 0.85f, passB = 0.75f; // PATH tile=8, VISITED tile=19
    // --- add

    // +++ add: XY preview marker uses PathPasser button color
    const float xyR = 0.20f, xyG = 0.85f, xyB = 0.75f;
    // center marker size (50%)
    const float xyShrink = 0.50f;
    // --- add

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + (float)c * cell;
            const float y0 = startY - (float)(r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = (uint8_t)grid[r][c];

            // +++ add: is this the UI-entered (X,Y)?
            // NOTE: uiStartX/uiStartY currently serve as the XY input boxes.
            const bool isXY = (c == uiStartX && r == uiStartY);
            // --- add

            if (v == 27)
            {
                PushRect_(verts, x0, y0, x1, y1, wallR, wallG, wallB, opaqueA);

                const float shrink = 0.50f;
                const float pad = cell * (1.0f - shrink) * 0.5f;
                PushRect_(verts, x0 + pad, y0 + pad, x1 - pad, y1 - pad, bfs2R, bfs2G, bfs2B, opaqueA);

                // +++ add: overlay XY marker on top
                if (isXY)
                {
                    const float pad2 = cell * (1.0f - xyShrink) * 0.5f;
                    PushRect_(verts, x0 + pad2, y0 + pad2, x1 - pad2, y1 - pad2, xyR, xyG, xyB, opaqueA);
                }
                // --- add
                continue;
            }

            if (v == 18)
            {
                // base wall tile (opaque)
                PushRect_(verts, x0, y0, x1, y1, wallR, wallG, wallB, opaqueA);

                // center overlay: 50% size, BREAK button color (opaque)
                const float shrink = 0.50f;
                const float pad = cell * (1.0f - shrink) * 0.5f;
                PushRect_(verts, x0 + pad, y0 + pad, x1 - pad, y1 - pad, bfs2R, bfs2G, bfs2B, opaqueA);

                // +++ add: overlay XY marker on top
                if (isXY)
                {
                    const float pad2 = cell * (1.0f - xyShrink) * 0.5f;
                    PushRect_(verts, x0 + pad2, y0 + pad2, x1 - pad2, y1 - pad2, xyR, xyG, xyB, opaqueA);
                }
                // --- add
                continue;
            }

            float rr = pathR, gg = pathG, bb = pathB, aa = opaqueA;

            if (v == 1) { rr = wallR; gg = wallG; bb = wallB; aa = opaqueA; }
            else if (v == 2) { rr = dfsR; gg = dfsG; bb = dfsB; aa = opaqueA; }
            else if (v == 3) { rr = bfsR; gg = bfsG; bb = bfsB; aa = opaqueA; }
            else if (v == 4) { rr = dijR; gg = dijG; bb = dijB; aa = opaqueA; }
            else if (v == 5) { rr = astR; gg = astG; bb = astB; aa = opaqueA; }
            else if (v == 6) { rr = floR; gg = floG; bb = floB; aa = opaqueA; }
            else if (v == 7) { rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = opaqueA; }

            // +++ add: PASS path tile
            else if (v == 8) { rr = passR; gg = passG; bb = passB; aa = opaqueA; }
            // --- add

            else if (v == 12) { rr = dfsR; gg = dfsG; bb = dfsB; aa = visitedA; }
            else if (v == 13) { rr = bfsR; gg = bfsG; bb = bfsB; aa = visitedA; }
            else if (v == 14) { rr = dijR; gg = dijG; bb = dijB; aa = visitedA; }
            else if (v == 15) { rr = astR; gg = astG; bb = astB; aa = visitedA; }
            else if (v == 16) { rr = floR; gg = floG; bb = floB; aa = visitedA; }
            else if (v == 17) { rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = visitedA; }

            // +++ add: PASS visited tile
            else if (v == 19) { rr = passR; gg = passG; bb = passB; aa = visitedA; }
            // --- add

            if (v == 6 && alphaOverrideActive)
            {
                const size_t idx = (size_t)r * (size_t)cols + (size_t)c;
                if (idx < cellAlphaOverride.size())
                    aa = std::clamp(cellAlphaOverride[idx], 0.0f, 1.0f);
            }

            PushRect_(verts, x0, y0, x1, y1, rr, gg, bb, aa);

            // +++ add: overlay XY marker on top of any tile (including walls)
            if (isXY)
            {
                const float pad2 = cell * (1.0f - xyShrink) * 0.5f;
                PushRect_(verts, x0 + pad2, y0 + pad2, x1 - pad2, y1 - pad2, xyR, xyG, xyB, opaqueA);
            }
            // --- add
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertexCount = (int)verts.size();
}

void Viewer::drawMaze()
{
    rebuildMeshIfDirty();

    if (vertexCount <= 0) return;

    // Draw maze into a square pixel viewport on the RIGHT side:
    // [fbW - sidePx, 0, sidePx, sidePx]
    const int sidePx = std::min(fbW, fbH);
    const int vpX = std::max(0, fbW - sidePx);
    const int vpY = 0;

    glViewport(vpX, vpY, sidePx, sidePx);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);

    // Restore full viewport for UI rendering
    glViewport(0, 0, fbW, fbH);
}