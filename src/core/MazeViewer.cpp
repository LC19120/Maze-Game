#include "core/Common.hpp"
#include "core/MazeViewer.hpp"

#include <iostream>
#include <vector>

#if __has_include(<glad/glad.h>)
  #include <glad/glad.h>
#else
  #error "glad/glad.h not found. Check ThirdPart/Mac/include path."
#endif

#if __has_include(<GFLW/glfw3.h>)
  #include <GFLW/glfw3.h>
#elif __has_include(<GLFW/glfw3.h>)
  #include <GLFW/glfw3.h>
#else
  #error "GLFW header not found (GFLW/glfw3.h or GLFW/glfw3.h)."
#endif

MazeViewer MazeViewer::getMazeViewer(Maze maze)
{
    MazeViewer viewer;
    viewer.maze = std::move(maze);
    return viewer;
}

static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048]{};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint LinkProgram(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048]{};
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::cerr << "Program link error: " << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void MazeViewer::displayMaze() const
{
    if (maze.grid.empty() || maze.grid[0].empty()) {
        std::cerr << "MazeViewer: maze grid is empty.\n";
        return;
    }

    if (!glfwInit()) {
        std::cerr << "glfwInit failed.\n";
        return;
    }

    // macOS core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(900, 900, "Maze (OpenGL)", nullptr, nullptr);
    if (!window) {
        std::cerr << "glfwCreateWindow failed.\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "gladLoadGLLoader failed. (Did you compile glad.c?)\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    const char* vsSrc = R"GLSL(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
    )GLSL";

    const char* fsSrc = R"GLSL(
        #version 330 core
        out vec4 FragColor;
        void main() { FragColor = vec4(0.0, 0.0, 0.0, 1.0); } // walls: black
    )GLSL";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    GLuint prog = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    const int rows = static_cast<int>(maze.grid.size());
    const int cols = static_cast<int>(maze.grid[0].size());

    // 只为“墙”生成三角形（两三角=1方块=6顶点，每顶点2 float）
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(rows * cols * 12)); // 大概值

    auto cellToNdc = [&](int r, int c, float& x0, float& y0, float& x1, float& y1) {
        // 映射到 [-1,1]，并让 r=0 在窗口顶部（y 从 1 到 -1）
        const float fx0 = static_cast<float>(c) / static_cast<float>(cols);
        const float fx1 = static_cast<float>(c + 1) / static_cast<float>(cols);
        const float fy0 = static_cast<float>(r) / static_cast<float>(rows);
        const float fy1 = static_cast<float>(r + 1) / static_cast<float>(rows);

        x0 = -1.0f + 2.0f * fx0;
        x1 = -1.0f + 2.0f * fx1;

        // 注意翻转 y
        y0 =  1.0f - 2.0f * fy0;
        y1 =  1.0f - 2.0f * fy1;
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (maze.grid[r][c] == 0) continue; // 路不画

            float x0, y0, x1, y1;
            cellToNdc(r, c, x0, y0, x1, y1);

            // 两个三角形： (x0,y0)-(x1,y0)-(x1,y1) 和 (x0,y0)-(x1,y1)-(x0,y1)
            verts.insert(verts.end(), {
                x0, y0,  x1, y0,  x1, y1,
                x0, y0,  x1, y1,  x0, y1
            });
        }
    }

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);

        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 2));

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);

    glfwDestroyWindow(window);
    glfwTerminate();
}