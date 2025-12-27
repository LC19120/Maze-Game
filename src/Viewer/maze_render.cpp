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

    // --- Layout in a square NDC space [-1, 1]x[-1, 1].
    // NOTE: drawMaze_() will set a *square pixel viewport* on the right side.
    const float cell = 2.0f / (float)std::max(rows, cols);

    const float totalW = cell * (float)cols;
    const float totalH = cell * (float)rows;

    const float startX = -1.0f + (2.0f - totalW) * 0.5f;
    const float startY =  1.0f - (2.0f - totalH) * 0.5f; // top for row 0
    // --- end layout

    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;

    const float dfsR = 0.95f, dfsG = 0.20f, dfsB = 0.20f; // 2  DFS
    const float bfsR = 1.00f, bfsG = 0.55f, bfsB = 0.05f; // 3  BFS
    const float bfs2R= 1.00f, bfs2G= 0.92f, bfs2B= 0.10f; // 7  BFS+
    const float dijR = 0.20f, dijG = 0.85f, dijB = 0.25f; // 4  Dijkstra
    const float astR = 0.20f, astG = 0.55f, astB = 1.00f; // 5  A*
    const float floR = 0.65f, floG = 0.25f, floB = 0.95f; // 6  Floyd

    const float visitedA = 0.50f; // was 0.28f
    const float opaqueA  = 1.00f;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + (float)c * cell;
            const float y0 = startY - (float)(r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = (uint8_t)grid[r][c];

            if (v == 27)
            {
                PushRect_(verts, x0, y0, x1, y1, wallR, wallG, wallB, opaqueA);

                const float shrink = 0.50f;
                const float pad = cell * (1.0f - shrink) * 0.5f;
                PushRect_(verts, x0 + pad, y0 + pad, x1 - pad, y1 - pad, bfs2R, bfs2G, bfs2B, opaqueA);
                continue;
            }

            // +++ add: broken-wall-on-path marker (wall base + CENTER yellow 50% alpha)
            if (v == 18)
            {
                // base wall tile (opaque)
                PushRect_(verts, x0, y0, x1, y1, wallR, wallG, wallB, opaqueA);

                // center overlay (BREAK button color, 50% alpha)
                const float shrink = 0.50f;
                const float pad = cell * (1.0f - shrink) * 0.5f;
                PushRect_(verts, x0 + pad, y0 + pad, x1 - pad, y1 - pad, bfs2R, bfs2G, bfs2B, visitedA);
                continue;
            }
            // --- add

            float rr = pathR, gg = pathG, bb = pathB, aa = opaqueA;

            if (v == 1) { rr = wallR; gg = wallG; bb = wallB; aa = opaqueA; }
            else if (v == 2) { rr = dfsR; gg = dfsG; bb = dfsB; aa = opaqueA; }
            else if (v == 3) { rr = bfsR; gg = bfsG; bb = bfsB; aa = opaqueA; }
            else if (v == 4) { rr = dijR; gg = dijG; bb = dijB; aa = opaqueA; }
            else if (v == 5) { rr = astR; gg = astG; bb = astB; aa = opaqueA; }
            else if (v == 6) { rr = floR; gg = floG; bb = floB; aa = opaqueA; }
            else if (v == 7) { rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = opaqueA; }

            else if (v == 12) { rr = dfsR; gg = dfsG; bb = dfsB; aa = visitedA; }
            else if (v == 13) { rr = bfsR; gg = bfsG; bb = bfsB; aa = visitedA; }
            else if (v == 14) { rr = dijR; gg = dijG; bb = dijB; aa = visitedA; }
            else if (v == 15) { rr = astR; gg = astG; bb = astB; aa = visitedA; }
            else if (v == 16) { rr = floR; gg = floG; bb = floB; aa = visitedA; }
            else if (v == 17) { rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = visitedA; }

            else if (v == 18) { rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = visitedA; }

            if (v == 6 && alphaOverrideActive)
            {
                const size_t idx = (size_t)r * (size_t)cols + (size_t)c;
                if (idx < cellAlphaOverride.size())
                    aa = std::clamp(cellAlphaOverride[idx], 0.0f, 1.0f);
            }

            PushRect_(verts, x0, y0, x1, y1, rr, gg, bb, aa);
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