#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <iostream>
#include <algorithm>

static GLuint CompileShader_(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
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
    if (!success)
    {
        char infolog[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, infolog);
        std::cerr << "Program linking failed: " << infolog << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void FramebufferSizeCallback_(GLFWwindow* w, int width, int height)
{
    const int W = std::max(1, width);
    const int H = std::max(1, height);

    // IMPORTANT:
    // Do NOT call any gl* here. This callback can fire before gladLoadGLLoader(),
    // and GL functions are null -> pc=0 crash.
    if (auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w)))
        self->onFramebufferResized(W, H);
}

void Viewer::initWindowAndGL()
{
    if (!glfwInit())
        throw std::runtime_error("glfwInit failed");

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    fbW = 900;
    fbH = 600;

    GLFWwindow* win = glfwCreateWindow(fbW, fbH, "Maze Viewer", nullptr, nullptr);
    if (!win)
    {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwSetWindowAspectRatio(win, 3, 2);

    window = win;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(win, this);
    glfwSetFramebufferSizeCallback(win, FramebufferSizeCallback_);
    int initW = 1, initH = 1;
    glfwGetFramebufferSize(win, &initW, &initH);
    onFramebufferResized(initW, initH);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(win);
        window = nullptr;
        glfwTerminate();
        throw std::runtime_error("gladLoadGLLoader failed");
    }

    glViewport(0, 0, fbW, fbH);

    const char* vsSrc = R"GLSL(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec4 aColor;   // +++ was vec3
        out vec4 vColor;                       // +++ was vec3
        void main() {
            vColor = aColor;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )GLSL";

    const char* fsSrc = R"GLSL(
        #version 330 core
        in vec4 vColor;        // +++
        out vec4 FragColor;
        void main() {
            FragColor = vColor; // +++ keep alpha
        }
    )GLSL";

    GLuint vs = CompileShader_(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader_(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        throw std::runtime_error("Shader compile failed");
    }

    program = LinkProgram_(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program)
        throw std::runtime_error("Program link failed");

    // +++ add: create VAO/VBO for maze and UI, and set vertex layout (pos2 + color4)
    auto setupStream = [](GLuint& outVao, GLuint& outVbo)
    {
        glGenVertexArrays(1, &outVao);
        glBindVertexArray(outVao);

        glGenBuffers(1, &outVbo);
        glBindBuffer(GL_ARRAY_BUFFER, outVbo);

        // location 0: vec2 aPos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, x));

        // location 1: vec4 aColor
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 4, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, r));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    };

    setupStream(vao, vbo);
    setupStream(uiVao, uiVbo);
    // --- add

    // enable alpha blending (safe now)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initUiCallbacks();
    updateWindowTitle();
}

void Viewer::shutdownGL()
{
    if (program) { glDeleteProgram(program); program = 0; }

    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }

    if (uiVbo) { glDeleteBuffers(1, &uiVbo); uiVbo = 0; }
    if (uiVao) { glDeleteVertexArrays(1, &uiVao); uiVao = 0; }

    if (window)
    {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window));
        window = nullptr;
        glfwTerminate();
    }
}