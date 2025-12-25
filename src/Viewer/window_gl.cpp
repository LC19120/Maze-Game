#include "Viewer/core.hpp"
#include "Viewer/ViewerInternal.hpp"

#include <iostream>

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

static void FramebufferSizeCallback_(GLFWwindow* /*w*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

void MazeViewer::initWindowAndGL_()
{
    if (!glfwInit())
        throw std::runtime_error("glfwInit failed");

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // optional: set a 12:9 initial size (4:3)
    fbW_ = 1200;
    fbH_ = 900;

    GLFWwindow* win = glfwCreateWindow(fbW_, fbH_, "Maze Viewer", nullptr, nullptr);
    if (!win)
    {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwSetWindowAspectRatio(win, 4, 3);

    window_ = win;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(win, FramebufferSizeCallback_);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(win);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("gladLoadGLLoader failed");
    }

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
    if (!vs || !fs)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        throw std::runtime_error("Shader compile failed");
    }

    program_ = LinkProgram_(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program_)
        throw std::runtime_error("Program link failed");

    // Maze VAO/VBO
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

    // UI VAO/VBO
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    initUiCallbacks_();
    updateWindowTitle_();
}

void MazeViewer::shutdownGL_()
{
    if (program_) { glDeleteProgram(program_); program_ = 0; }

    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }

    if (uiVbo_) { glDeleteBuffers(1, &uiVbo_); uiVbo_ = 0; }
    if (uiVao_) { glDeleteVertexArrays(1, &uiVao_); uiVao_ = 0; }

    if (window_)
    {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
        window_ = nullptr;
        glfwTerminate();
    }
}