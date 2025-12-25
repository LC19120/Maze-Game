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
    const float expR  = 0.20f, expG  = 0.55f, expB  = 0.95f; // explored (2)

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + c * cell;
            const float y0 = startY - (r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = grid[r][c];
            float rr = pathR, gg = pathG, bb = pathB;

            if (v == 1) { rr = wallR; gg = wallG; bb = wallB; }
            else if (v == 2) { rr = expR; gg = expG; bb = expB; }

            PushRect_(verts, x0, y0, x1, y1, rr, gg, bb);
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