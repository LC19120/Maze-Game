#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <algorithm>

// 重新构建迷宫渲染网格（如果迷宫数据或窗口尺寸有变化）
void Viewer::rebuildMesh()
{
    const bool fbChanged = (meshBuiltFbW != fbW) || (meshBuiltFbH != fbH);
    if (!(mazeLoaded && (mazeDirty || fbChanged))) return;

    meshBuiltFbW = fbW;
    meshBuiltFbH = fbH;
    mazeDirty = false;

    rebuildMeshFromMaze(maze);
}

// 根据迷宫数据生成顶点数组，并上传到 OpenGL 缓冲区
void Viewer::rebuildMeshFromMaze(const Maze& m)
{
    const auto& grid = m.grid;
    const int rows = (int)grid.size();
    if (rows <= 0) { vertexCount = 0; return; }
    const int cols = (int)grid[0].size();
    if (cols <= 0) { vertexCount = 0; return; }

    std::vector<Vertex> verts;
    verts.reserve((size_t)rows * (size_t)cols * 6);

    // 单元格大小和起始坐标
    const float cell = 2.0f / (float)std::max(rows, cols);
    const float totalW = cell * (float)cols;
    const float totalH = cell * (float)rows;
    const float startX = -1.0f + (2.0f - totalW) * 0.5f;
    const float startY =  1.0f - (2.0f - totalH) * 0.5f;

    // 颜色定义（只保留实际用到的算法颜色）
    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;
    const float bfs2R= 1.00f, bfs2G= 0.92f, bfs2B= 0.10f; // BREAK
    const float astR = 0.20f, astG = 0.55f, astB = 1.00f; // A*
    const float floR = 0.65f, floG = 0.25f, floB = 0.95f; // COUNT
    const float passR = 0.20f, passG = 0.85f, passB = 0.75f; // PASS
    const float xyR = 0.20f, xyG = 0.85f, xyB = 0.75f;
    const float xyShrink = 0.50f;
    const float visitedA = 0.50f;
    const float opaqueA  = 1.00f;

    // 绘制迷宫网格
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + (float)c * cell;
            const float y0 = startY - (float)(r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = (uint8_t)grid[r][c];
            const bool isXY = (c == uiStartX && r == uiStartY);

            // 特殊 tile 27: 墙体+BREAK覆盖
            if (v == 27 || v == 18)
            {
                PushRect(verts, x0, y0, x1, y1, wallR, wallG, wallB, opaqueA);
                const float shrink = 0.50f;
                const float pad = cell * (1.0f - shrink) * 0.5f;
                PushRect(verts, x0 + pad, y0 + pad, x1 - pad, y1 - pad, bfs2R, bfs2G, bfs2B, opaqueA);

                if (isXY)
                {
                    const float pad2 = cell * (1.0f - xyShrink) * 0.5f;
                    PushRect(verts, x0 + pad2, y0 + pad2, x1 - pad2, y1 - pad2, xyR, xyG, xyB, opaqueA);
                }
                continue;
            }

            // 普通单元格
            float rr = pathR, gg = pathG, bb = pathB, aa = opaqueA;
            switch (v) {
                case 1:  rr = wallR; gg = wallG; bb = wallB; aa = opaqueA; break;
                case 5:  rr = astR;  gg = astG;  bb = astB;  aa = opaqueA; break;
                case 6:  rr = floR;  gg = floG;  bb = floB;  aa = opaqueA; break;
                case 7:  rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = opaqueA; break;
                case 8:  rr = passR; gg = passG; bb = passB; aa = opaqueA; break;
                case 15: rr = astR;  gg = astG;  bb = astB;  aa = visitedA; break;
                case 16: rr = floR;  gg = floG;  bb = floB;  aa = visitedA; break;
                case 17: rr = bfs2R; gg = bfs2G; bb = bfs2B; aa = visitedA; break;
                case 19: rr = passR; gg = passG; bb = passB; aa = visitedA; break;
            }

            // Floyd 算法支持透明度覆盖
            if (v == 6 && alphaOverrideActive)
            {
                const size_t idx = (size_t)r * (size_t)cols + (size_t)c;
                if (idx < cellAlphaOverride.size())
                    aa = std::clamp(cellAlphaOverride[idx], 0.0f, 1.0f);
            }

            PushRect(verts, x0, y0, x1, y1, rr, gg, bb, aa);

            if (isXY)
            {
                const float pad2 = cell * (1.0f - xyShrink) * 0.5f;
                PushRect(verts, x0 + pad2, y0 + pad2, x1 - pad2, y1 - pad2, xyR, xyG, xyB, opaqueA);
            }
        }
    }

    // 上传顶点数据到 OpenGL 缓冲区
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertexCount = (int)verts.size();
}

// 绘制迷宫到屏幕右侧的正方形区域
void Viewer::drawMaze()
{
    rebuildMesh();

    if (vertexCount <= 0) return;

    // 计算迷宫区域像素大小和位置
    const int sidePx = std::min(fbW, fbH);
    const int vpX = std::max(0, fbW - sidePx);
    const int vpY = 0;

    // 设置 OpenGL 视口为迷宫区域
    glViewport(vpX, vpY, sidePx, sidePx);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);

    // 恢复全窗口视口用于 UI 渲染
    glViewport(0, 0, fbW, fbH);
}