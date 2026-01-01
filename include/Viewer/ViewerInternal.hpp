#pragma once
#include "core/Common.hpp"
#include "core/MazeBuilder.hpp" // add: for MazeType

#if __has_include(<glad/glad.h>)
  #include <glad/glad.h>
#else
  #error "glad/glad.h not found. Check ThirdPart/Mac/include path."
#endif

#if __has_include(<GLFW/glfw3.h>)
  #include <GLFW/glfw3.h>
#elif __has_include(<GFLW/glfw3.h>)
  #include <GFLW/glfw3.h>
#else
  #error "GLFW header not found."
#endif

struct Vertex
{
    float x, y;
    float r, g, b, a;
};

// 向顶点数组添加一个矩形
inline void PushRect(std::vector<Vertex>& out,
                      float x0, float y0, float x1, float y1,
                      float r, float g, float b,
                      float a = 1.0f)
{
    out.push_back({x0, y0, r, g, b, a});
    out.push_back({x1, y0, r, g, b, a});
    out.push_back({x1, y1, r, g, b, a});

    out.push_back({x0, y0, r, g, b, a});
    out.push_back({x1, y1, r, g, b, a});
    out.push_back({x0, y1, r, g, b, a});
}

// 判断点 (mx, my) 是否在矩形
inline bool Hit(float mx, float my, float x0, float y0, float x1, float y1)
{
    return mx >= x0 && mx <= x1 && my >= y0 && my <= y1;
}