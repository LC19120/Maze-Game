#include "core/Common.hpp"
#include "core/MazeViewer.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

#if __has_include(<glad/glad.h>)
  #include <glad/glad.h>
#else
  #error "glad/glad.h not found. Check ThirdPart/Mac/include path."
#endif

#if __has_include(<GLFW/glfw3.h>)
  #include <GLFW/glfw3.h>
#elif __has_include(<GFLW/glfw3.h>)
  // 兼容你工程里可能存在的非标准路径（注意：GFLW 很可能是拼写错误）
  #include <GFLW/glfw3.h>
#else
  #error "GLFW header not found."
#endif

struct Vertex {
    float x, y;
    float r, g, b;
};

static GLuint CompileShader_(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infolog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infolog);
        std::cerr << "Shader compilation failed: " << infolog << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram_(GLuint vs, GLuint fs)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    int success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infolog[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, infolog);
        std::cerr << "Program linking failed: " << infolog << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void FramebufferSizeCallback_(GLFWwindow* /*w*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

MazeViewer& MazeViewer::getInstance()
{
    static MazeViewer instance;
    return instance;
}

MazeViewer::MazeViewer() = default;

MazeViewer::~MazeViewer()
{
    requestClose();
    shutdownGL_();
}

void MazeViewer::setMaze(const Maze& newMaze)
{
    std::lock_guard<std::mutex> lock(mazeMutex_);
    maze_ = newMaze;
    mazeLoaded_ = true;
    mazeDirty_ = true;
}

bool MazeViewer::isOpen() const
{
    return window_ != nullptr && glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)) == 0;
}

void MazeViewer::requestClose()
{
    closeRequested_.store(true, std::memory_order_relaxed);
}

void MazeViewer::initWindowAndGL_()
{
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* win = glfwCreateWindow(fbW_, fbH_, "Maze Viewer", nullptr, nullptr);
    if (!win) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    window_ = win;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // vsync
    glfwSetFramebufferSizeCallback(win, FramebufferSizeCallback_);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(win);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("gladLoadGLLoader failed");
    }

    glViewport(0, 0, fbW_, fbH_);

    const char* vsSrc = R"GLSL(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            vColor = aColor;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )GLSL";

    const char* fsSrc = R"GLSL(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )GLSL";

    GLuint vs = CompileShader_(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader_(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        throw std::runtime_error("Shader compile failed");
    }

    program_ = LinkProgram_(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program_) {
        throw std::runtime_error("Program link failed");
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MazeViewer::shutdownGL_()
{
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }

    if (window_) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
        window_ = nullptr;
        glfwTerminate();
    }
}

void MazeViewer::processEvents_()
{
    glfwPollEvents();
}

static void PushCell_(std::vector<Vertex>& out,
                      float x0, float y0, float x1, float y1,
                      float r, float g, float b)
{
    // two triangles
    out.push_back({x0, y0, r, g, b});
    out.push_back({x1, y0, r, g, b});
    out.push_back({x1, y1, r, g, b});

    out.push_back({x0, y0, r, g, b});
    out.push_back({x1, y1, r, g, b});
    out.push_back({x0, y1, r, g, b});
}

void MazeViewer::rebuildMeshFromMaze_(const Maze& m)
{
    const auto& grid = m.grid;
    const int rows = static_cast<int>(grid.size());
    if (rows <= 0) { vertexCount_ = 0; return; }
    const int cols = static_cast<int>(grid[0].size());
    if (cols <= 0) { vertexCount_ = 0; return; }

    std::vector<Vertex> verts;
    verts.reserve(static_cast<size_t>(rows) * static_cast<size_t>(cols) * 6);

    // fit grid into NDC [-1,1] keeping aspect
    const float aspect = (fbH_ == 0) ? 1.0f : (static_cast<float>(fbW_) / static_cast<float>(fbH_));
    float gridW = 2.0f;
    float gridH = 2.0f;

    // scale so cells stay square in NDC
    if (aspect >= 1.0f) {
        gridW = 2.0f * aspect;
        gridH = 2.0f;
    } else {
        gridW = 2.0f;
        gridH = 2.0f / aspect;
    }

    const float cellW = gridW / static_cast<float>(cols);
    const float cellH = gridH / static_cast<float>(rows);
    const float cell = std::min(cellW, cellH);

    const float totalW = cell * cols;
    const float totalH = cell * rows;
    const float startX = -totalW * 0.5f;
    const float startY =  totalH * 0.5f;

    // colors
    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const float x0 = startX + c * cell;
            const float y0 = startY - (r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const bool isWall = (grid[r][c] == 1);
            PushCell_(verts, x0, y0, x1, y1,
                      isWall ? wallR : pathR,
                      isWall ? wallG : pathG,
                      isWall ? wallB : pathB);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertexCount_ = static_cast<int>(verts.size());
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

    if (doRebuild) {
        rebuildMeshFromMaze_(local);
    }
}

void MazeViewer::renderFrame_()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &w, &h);
    fbW_ = w; fbH_ = h;

    rebuildMeshIfDirty_();

    glClearColor(0.90f, 0.90f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (vertexCount_ > 0) {
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    glfwSwapBuffers(static_cast<GLFWwindow*>(window_));
}

void MazeViewer::run()
{
    initWindowAndGL_();

    while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)))
    {
        if (closeRequested_.load(std::memory_order_relaxed)) {
            glfwSetWindowShouldClose(static_cast<GLFWwindow*>(window_), GLFW_TRUE);
        }

        processEvents_();
        renderFrame_();
    }

    shutdownGL_();
}