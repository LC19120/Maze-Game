#include "core/Common.hpp"
#include "core/MazeViewer.hpp"
#include "core/PathFinder.hpp"

#if __has_include(<glad/glad.h>)
#include <glad/glad.h>
#else
#error "glad/glad.h not found. Check ThirdPart/Mac/include path."
#endif

#if __has_include(<GLFW/glfw3.h>)
#include <GLFW/glfw3.h>
#else
#error "GLFW header not found."
#endif

struct Vertex
{
    float x, y;
    float r, g, b;
};

static GLuint CompileShader_(GLenum type, const char *src)
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

static void FramebufferSizeCallback_(GLFWwindow * /*w*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

static MazeType UiIndexToType_(int idx)
{
    switch (idx)
    {
    case 0:
        return MazeType::Small;
    case 1:
        return MazeType::Medium;
    case 2:
        return MazeType::Large;
    case 3:
        return MazeType::Ultra;
    default:
        return MazeType::Small;
    }
}

MazeViewer &MazeViewer::getInstance()
{
    static MazeViewer instance;
    return instance;
}

MazeViewer::MazeViewer()
    : pool_(std::max(1u, std::thread::hardware_concurrency() > 2
                             ? std::thread::hardware_concurrency() - 2
                             : 1u))
{
}

MazeViewer::~MazeViewer()
{
    cancelWork_();
    requestClose();
    shutdownGL_();
}

void MazeViewer::setMaze(const Maze &newMaze)
{
    std::lock_guard<std::mutex> lock(mazeMutex_);
    maze_ = newMaze;
    mazeLoaded_ = true;
    mazeDirty_ = true;
}

bool MazeViewer::isOpen() const
{
    return window_ != nullptr && glfwWindowShouldClose(static_cast<GLFWwindow *>(window_)) == 0;
}

void MazeViewer::requestClose()
{
    closeRequested_.store(true, std::memory_order_relaxed);
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

    GLFWwindow *win = glfwCreateWindow(fbW_, fbH_, "Maze Viewer", nullptr, nullptr);
    if (!win)
    {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

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

    const char *vsSrc = R"GLSL(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            vColor = aColor;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )GLSL";

    const char *fsSrc = R"GLSL(
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
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, r));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // UI VAO/VBO
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, r));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    initUiCallbacks_();
    updateWindowTitle_();
}

void MazeViewer::shutdownGL_()
{
    if (program_)
    {
        glDeleteProgram(program_);
        program_ = 0;
    }

    if (vbo_)
    {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_)
    {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    if (uiVbo_)
    {
        glDeleteBuffers(1, &uiVbo_);
        uiVbo_ = 0;
    }
    if (uiVao_)
    {
        glDeleteVertexArrays(1, &uiVao_);
        uiVao_ = 0;
    }

    if (window_)
    {
        glfwDestroyWindow(static_cast<GLFWwindow *>(window_));
        window_ = nullptr;
        glfwTerminate();
    }
}

void MazeViewer::processEvents_()
{
    glfwPollEvents();
}

static void PushRect_(std::vector<Vertex> &out,
                      float x0, float y0, float x1, float y1,
                      float r, float g, float b)
{
    out.push_back({x0, y0, r, g, b});
    out.push_back({x1, y0, r, g, b});
    out.push_back({x1, y1, r, g, b});

    out.push_back({x0, y0, r, g, b});
    out.push_back({x1, y1, r, g, b});
    out.push_back({x0, y1, r, g, b});
}

static bool Hit_(float mx, float my, float x0, float y0, float x1, float y1)
{
    return mx >= x0 && mx <= x1 && my >= y0 && my <= y1;
}

// 7-seg digit renderer (no text libs)
// segments: A(top) B(ur) C(lr) D(bot) E(ll) F(ul) G(mid)
static void PushSeg_(std::vector<Vertex> &out, float x0, float y0, float x1, float y1, float r, float g, float b)
{
    PushRect_(out, x0, y0, x1, y1, r, g, b);
}

static void PushDigit7_(std::vector<Vertex> &out, char ch,
                        float x, float y, float w, float h,
                        float r, float g, float b)
{
    const float t = std::min(w, h) * 0.18f; // thickness
    const float x0 = x, x1 = x + w;
    const float y0 = y, y1 = y + h;

    // segment rects
    const float ax0 = x0 + t, ax1 = x1 - t, ay0 = y1 - t, ay1 = y1;
    const float dx0 = x0 + t, dx1 = x1 - t, dy0 = y0, dy1 = y0 + t;
    const float gx0 = x0 + t, gx1 = x1 - t, gy0 = y0 + (h - t) * 0.5f, gy1 = gy0 + t;

    const float fx0 = x0, fx1 = x0 + t, fy0 = y0 + (h * 0.5f), fy1 = y1 - t;
    const float ex0 = x0, ex1 = x0 + t, ey0 = y0 + t, ey1 = y0 + (h * 0.5f);

    const float bx0 = x1 - t, bx1 = x1, by0 = y0 + (h * 0.5f), by1 = y1 - t;
    const float cx0 = x1 - t, cx1 = x1, cy0 = y0 + t, cy1 = y0 + (h * 0.5f);

    auto segA = [&]
    { PushSeg_(out, ax0, ay0, ax1, ay1, r, g, b); };
    auto segB = [&]
    { PushSeg_(out, bx0, by0, bx1, by1, r, g, b); };
    auto segC = [&]
    { PushSeg_(out, cx0, cy0, cx1, cy1, r, g, b); };
    auto segD = [&]
    { PushSeg_(out, dx0, dy0, dx1, dy1, r, g, b); };
    auto segE = [&]
    { PushSeg_(out, ex0, ey0, ex1, ey1, r, g, b); };
    auto segF = [&]
    { PushSeg_(out, fx0, fy0, fx1, fy1, r, g, b); };
    auto segG = [&]
    { PushSeg_(out, gx0, gy0, gx1, gy1, r, g, b); };

    auto on = [&](bool A, bool B, bool C, bool D, bool E, bool F, bool G)
    {
        if (A)
            segA();
        if (B)
            segB();
        if (C)
            segC();
        if (D)
            segD();
        if (E)
            segE();
        if (F)
            segF();
        if (G)
            segG();
    };

    switch (ch)
    {
    case '0':
        on(true, true, true, true, true, true, false);
        break;
    case '1':
        on(false, true, true, false, false, false, false);
        break;
    case '2':
        on(true, true, false, true, true, false, true);
        break;
    case '3':
        on(true, true, true, true, false, false, true);
        break;
    case '4':
        on(false, true, true, false, false, true, true);
        break;
    case '5':
        on(true, false, true, true, false, true, true);
        break;
    case '6':
        on(true, false, true, true, true, true, true);
        break;
    case '7':
        on(true, true, true, false, false, false, false);
        break;
    case '8':
        on(true, true, true, true, true, true, true);
        break;
    case '9':
        on(true, true, true, true, false, true, true);
        break;
    case '-':
        on(false, false, false, false, false, false, true);
        break;
    default:
        break;
    }
}

static void PushInt7_(std::vector<Vertex> &out, int v,
                      float x, float y, float w, float h,
                      float r, float g, float b)
{
    std::string s = std::to_string(v);
    const float gap = w * 0.12f;
    const float cw = (w - gap * (float)(std::max<size_t>(s.size(), 1) - 1)) / (float)std::max<size_t>(s.size(), 1);
    float cx = x;
    for (char ch : s)
    {
        PushDigit7_(out, ch, cx, y, cw, h, r, g, b);
        cx += cw + gap;
    }
}

void MazeViewer::rebuildMeshFromMaze_(const Maze &m)
{
    const auto &grid = m.grid;
    const int rows = static_cast<int>(grid.size());
    if (rows <= 0)
    {
        vertexCount_ = 0;
        return;
    }
    const int cols = static_cast<int>(grid[0].size());
    if (cols <= 0)
    {
        vertexCount_ = 0;
        return;
    }

    std::vector<Vertex> verts;
    verts.reserve(static_cast<size_t>(rows) * static_cast<size_t>(cols) * 6);

    const float aspect = (fbH_ == 0) ? 1.0f : (static_cast<float>(fbW_) / static_cast<float>(fbH_));
    float gridW = (aspect >= 1.0f) ? (2.0f * aspect) : 2.0f;
    float gridH = (aspect >= 1.0f) ? 2.0f : (2.0f / aspect);

    const float cellW = gridW / static_cast<float>(cols);
    const float cellH = gridH / static_cast<float>(rows);
    const float cell = std::min(cellW, cellH);

    const float totalW = cell * cols;
    const float totalH = cell * rows;
    const float startX = -totalW * 0.5f;
    const float startY = totalH * 0.5f;

    const float wallR = 0.05f, wallG = 0.05f, wallB = 0.05f;
    const float pathR = 0.95f, pathG = 0.95f, pathB = 0.95f;
    const float expR = 0.20f, expG = 0.55f, expB = 0.95f; // explored (2)

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float x0 = startX + c * cell;
            const float y0 = startY - (r + 1) * cell;
            const float x1 = x0 + cell;
            const float y1 = y0 + cell;

            const uint8_t v = grid[r][c];
            const bool isWall = (v == 1);
            const bool isExplored = (v == 2);

            float rr = pathR, gg = pathG, bb = pathB;
            if (isWall)
            {
                rr = wallR;
                gg = wallG;
                bb = wallB;
            }
            else if (isExplored)
            {
                rr = expR;
                gg = expG;
                bb = expB;
            }

            PushRect_(verts, x0, y0, x1, y1, rr, gg, bb);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(Vertex)),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertexCount_ = (int)verts.size();
}

void MazeViewer::rebuildMeshIfDirty_()
{
    Maze local{};
    bool doRebuild = false;

    {
        std::lock_guard<std::mutex> lock(mazeMutex_);
        if (mazeLoaded_ && mazeDirty_)
        {
            local = maze_;
            mazeDirty_ = false;
            doRebuild = true;
        }
    }

    if (doRebuild)
        rebuildMeshFromMaze_(local);
}

void MazeViewer::cancelWork_()
{
    std::lock_guard<std::mutex> lk(cancelMutex_);
    if (cancelFlag_)
        cancelFlag_->store(true, std::memory_order_relaxed);
    cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
}

void MazeViewer::updateWindowTitle_()
{
    std::ostringstream oss;
    oss << "Maze Viewer | "
        << "Type=" << (uiTypeIndex_ + 1)
        << " Seed=" << uiSeed_
        << " Start=(" << uiStartX_ << "," << uiStartY_ << ")"
        << " End=(" << uiEndX_ << "," << uiEndY_ << ")"
        << " | Click boxes to edit digits, [B]=Build [F]=Find [ESC]=Cancel";
    glfwSetWindowTitle(static_cast<GLFWwindow *>(window_), oss.str().c_str());
}

void MazeViewer::applyEdit_()
{
    auto parseInt = [&](int &dst)
    {
        if (uiEdit_.empty() || uiEdit_ == "-")
            return;
        try
        {
            dst = std::stoi(uiEdit_);
        }
        catch (...)
        {
        }
    };

    switch (uiFocus_)
    {
    case UiField::Seed:
        parseInt(uiSeed_);
        break;
    case UiField::StartX:
        parseInt(uiStartX_);
        break;
    case UiField::StartY:
        parseInt(uiStartY_);
        break;
    case UiField::EndX:
        parseInt(uiEndX_);
        break;
    case UiField::EndY:
        parseInt(uiEndY_);
        break;
    case UiField::UpdateEvery:
        parseInt(uiUpdateEvery_);
        break;
    case UiField::DelayMs:
        parseInt(uiDelayMs_);
        break;
    default:
        break;
    }
}

void MazeViewer::initUiCallbacks_()
{
    GLFWwindow *win = static_cast<GLFWwindow *>(window_);
    glfwSetWindowUserPointer(win, this);

    glfwSetCharCallback(win, [](GLFWwindow *w, unsigned int codepoint)
                        {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (self->uiFocus_ == UiField::None) return;

        if (codepoint > 127) return;
        const char ch = (char)codepoint;
        if (ch >= '0' && ch <= '9') {
            if (self->uiEdit_.size() < 12) self->uiEdit_.push_back(ch);
        } else if (ch == '-') {
            if (self->uiEdit_.empty()) self->uiEdit_.push_back(ch);
        } });

    glfwSetKeyCallback(win, [](GLFWwindow *w, int key, int /*scancode*/, int action, int /*mods*/)
                       {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

        if (key == GLFW_KEY_ESCAPE) {
            self->cancelWork_();
            self->uiFocus_ = UiField::None;
            self->uiEdit_.clear();
            {
                std::lock_guard<std::mutex> lk(self->uiMsgMutex_);
                self->uiLastMsg_ = "Cancelled.";
            }
            self->updateWindowTitle_();
            return;
        }

        if (self->uiFocus_ != UiField::None) {
            if (key == GLFW_KEY_BACKSPACE) {
                if (!self->uiEdit_.empty()) self->uiEdit_.pop_back();
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                self->applyEdit_();
                self->uiFocus_ = UiField::None;
                self->uiEdit_.clear();
                self->updateWindowTitle_();
            }
            return;
        }

        if (key == GLFW_KEY_B) {
            self->requestBuild_(UiIndexToType_(self->uiTypeIndex_), self->uiSeed_);
            return;
        }
        if (key == GLFW_KEY_F) {
            self->requestFindPath_(self->uiStartX_, self->uiStartY_, self->uiEndX_, self->uiEndY_);
            return;
        } });

    glfwSetMouseButtonCallback(win, [](GLFWwindow *w, int button, int action, int /*mods*/)
    {
        auto* self = static_cast<MazeViewer*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

        double px = 0, py = 0;
        glfwGetCursorPos(w, &px, &py);

        // IMPORTANT: cursor pos is in window coordinates, so use window size (not framebuffer size)
        int winW = 1, winH = 1;
        glfwGetWindowSize(w, &winW, &winH);
        if (winW <= 0) winW = 1;
        if (winH <= 0) winH = 1;

        const float mx = (float)((px / (double)winW) * 2.0 - 1.0);
        const float my = (float)(1.0 - (py / (double)winH) * 2.0);

        // UI layout in NDC
        const float panelX0 = -1.0f, panelX1 = -0.35f;
        const float yTop = 0.95f;
        const float rowH = 0.12f;
        const float boxX0 = panelX0 + 0.05f;
        const float boxX1 = panelX1 - 0.05f;

        auto setFocus = [&](UiField f, int current){
            self->uiFocus_ = f;
            self->uiEdit_ = std::to_string(current);
            self->updateWindowTitle_();
        };

        // Buttons row
        const float btnY1 = yTop;
        const float btnY0 = yTop - rowH;
        const float btnW = (panelX1 - panelX0 - 0.10f - 0.02f*2.0f) / 3.0f;
        const float bx0 = panelX0 + 0.05f;
        const float b1x0 = bx0;
        const float b1x1 = b1x0 + btnW;
        const float b2x0 = b1x1 + 0.02f;
        const float b2x1 = b2x0 + btnW;
        const float b3x0 = b2x1 + 0.02f;
        const float b3x1 = b3x0 + btnW;

        if (Hit_(mx, my, b1x0, btnY0, b1x1, btnY1)) { self->requestBuild_(UiIndexToType_(self->uiTypeIndex_), self->uiSeed_); return; }
        if (Hit_(mx, my, b2x0, btnY0, b2x1, btnY1)) { self->requestFindPath_(self->uiStartX_, self->uiStartY_, self->uiEndX_, self->uiEndY_); return; }
        if (Hit_(mx, my, b3x0, btnY0, b3x1, btnY1)) {
            self->cancelWork_();
            std::lock_guard<std::mutex> lk(self->uiMsgMutex_);
            self->uiLastMsg_ = "Cancelled.";
            return;
        }

        // Type selector (4 small boxes)
        const float typeY1 = btnY0 - 0.02f;
        const float typeY0 = typeY1 - rowH;
        const float tW = (boxX1 - boxX0 - 0.02f*3.0f) / 4.0f;
        for (int i = 0; i < 4; ++i) {
            float tx0 = boxX0 + i * (tW + 0.02f);
            float tx1 = tx0 + tW;
            if (Hit_(mx, my, tx0, typeY0, tx1, typeY1)) {
                self->uiTypeIndex_ = i;
                self->updateWindowTitle_();
                return;
            }
        }

        // Numeric boxes
        auto boxRow = [&](int idx, UiField field, int cur){
            const float y1 = typeY0 - 0.02f - idx * (rowH + 0.02f);
            const float y0 = y1 - rowH;
            if (Hit_(mx, my, boxX0, y0, boxX1, y1)) setFocus(field, cur);
        };

        int r = 0;
        boxRow(r++, UiField::Seed,       self->uiSeed_);
        boxRow(r++, UiField::StartX,     self->uiStartX_);
        boxRow(r++, UiField::StartY,     self->uiStartY_);
        boxRow(r++, UiField::EndX,       self->uiEndX_);
        boxRow(r++, UiField::EndY,       self->uiEndY_);
        boxRow(r++, UiField::UpdateEvery,self->uiUpdateEvery_);
        boxRow(r++, UiField::DelayMs,    self->uiDelayMs_); 
    });
}

void MazeViewer::renderUi_()
{
    // Build UI vertices each frame
    std::vector<Vertex> ui;
    ui.reserve(2000);

    const float panelX0 = -1.0f, panelX1 = -0.35f;
    const float panelY0 = -1.0f, panelY1 = 1.0f;

    // panel background
    PushRect_(ui, panelX0, panelY0, panelX1, panelY1, 0.12f, 0.12f, 0.12f);

    // helpers
    auto drawBox = [&](float x0, float y0, float x1, float y1, bool focused)
    {
        const float br = focused ? 0.95f : 0.35f;
        const float bg = focused ? 0.85f : 0.35f;
        const float bb = focused ? 0.20f : 0.35f;
        // border
        PushRect_(ui, x0 - 0.005f, y0 - 0.005f, x1 + 0.005f, y1 + 0.005f, br, bg, bb);
        // fill
        PushRect_(ui, x0, y0, x1, y1, 0.18f, 0.18f, 0.18f);
    };

    const float yTop = 0.95f;
    const float rowH = 0.12f;

    // Buttons: Build / Find / Cancel
    const float btnY1 = yTop;
    const float btnY0 = yTop - rowH;
    const float btnW = (panelX1 - panelX0 - 0.10f - 0.02f * 2.0f) / 3.0f;
    const float bx0 = panelX0 + 0.05f;
    const float b1x0 = bx0, b1x1 = b1x0 + btnW;
    const float b2x0 = b1x1 + 0.02f, b2x1 = b2x0 + btnW;
    const float b3x0 = b2x1 + 0.02f, b3x1 = b3x0 + btnW;

    PushRect_(ui, b1x0, btnY0, b1x1, btnY1, 0.20f, 0.70f, 0.25f); // Build
    PushRect_(ui, b2x0, btnY0, b2x1, btnY1, 0.20f, 0.55f, 0.95f); // Find
    PushRect_(ui, b3x0, btnY0, b3x1, btnY1, 0.90f, 0.30f, 0.30f); // Cancel

    // Type selector (4 boxes), show 1..4 as digits
    const float boxX0 = panelX0 + 0.05f;
    const float boxX1 = panelX1 - 0.05f;
    const float typeY1 = btnY0 - 0.02f;
    const float typeY0 = typeY1 - rowH;
    const float tW = (boxX1 - boxX0 - 0.02f * 3.0f) / 4.0f;

    for (int i = 0; i < 4; ++i)
    {
        float tx0 = boxX0 + i * (tW + 0.02f);
        float tx1 = tx0 + tW;
        const bool sel = (uiTypeIndex_ == i);
        PushRect_(ui, tx0, typeY0, tx1, typeY1, sel ? 0.85f : 0.30f, sel ? 0.70f : 0.30f, sel ? 0.20f : 0.30f);
        PushDigit7_(ui, char('1' + i), tx0 + 0.02f, typeY0 + 0.02f, (tx1 - tx0) - 0.04f, (typeY1 - typeY0) - 0.04f, 0.05f, 0.05f, 0.05f);
    }

    // Numeric rows
    auto rowY = [&](int idx, float &y0, float &y1)
    {
        y1 = typeY0 - 0.02f - idx * (rowH + 0.02f);
        y0 = y1 - rowH;
    };

    auto valueForField = [&](UiField f) -> int
    {
        switch (f)
        {
        case UiField::Seed:
            return uiSeed_;
        case UiField::StartX:
            return uiStartX_;
        case UiField::StartY:
            return uiStartY_;
        case UiField::EndX:
            return uiEndX_;
        case UiField::EndY:
            return uiEndY_;
        case UiField::UpdateEvery:
            return uiUpdateEvery_;
        case UiField::DelayMs:
            return uiDelayMs_;
        default:
            return 0;
        }
    };

    UiField fields[] = {UiField::Seed, UiField::StartX, UiField::StartY, UiField::EndX, UiField::EndY, UiField::UpdateEvery, UiField::DelayMs};

    for (int i = 0; i < (int)(sizeof(fields) / sizeof(fields[0])); ++i)
    {
        float y0, y1;
        rowY(i, y0, y1);
        const bool focused = (uiFocus_ == fields[i]);
        drawBox(boxX0, y0, boxX1, y1, focused);

        const int v = (focused && !uiEdit_.empty()) ? ([](const std::string &s)
                                                       { try { return std::stoi(s); } catch (...) { return 0; } }(uiEdit_))
                                                    : valueForField(fields[i]);

        PushInt7_(ui, v, boxX0 + 0.02f, y0 + 0.02f, (boxX1 - boxX0) - 0.04f, (y1 - y0) - 0.04f,
                  0.92f, 0.92f, 0.92f);
    }

    // Upload + draw
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui.size() * sizeof(Vertex)), ui.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    uiVertexCount_ = (int)ui.size();

    if (uiVertexCount_ > 0)
    {
        glUseProgram(program_);
        glBindVertexArray(uiVao_);
        glDrawArrays(GL_TRIANGLES, 0, uiVertexCount_);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}

void MazeViewer::renderFrame_()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow *>(window_), &w, &h);
    fbW_ = w;
    fbH_ = h;

    rebuildMeshIfDirty_();

    glClearColor(0.90f, 0.90f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw maze
    if (vertexCount_ > 0)
    {
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // draw UI on top
    renderUi_();

    glfwSwapBuffers(static_cast<GLFWwindow *>(window_));
}

// --- add: missing definitions (fix undefined symbols) --------------------
void MazeViewer::requestBuild_(MazeType type, int seed)
{
    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    // replace any previous work
    std::shared_ptr<std::atomic<bool>> myCancel;
    {
        std::lock_guard<std::mutex> lk(cancelMutex_);
        cancelFlag_->store(true, std::memory_order_relaxed);
        cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        myCancel = cancelFlag_;
    }

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Building...";
    }

    // run in background (no dependency on ThreadPool API)
    std::thread([this, type, seed, myToken, myCancel] {
        try {
            MazeBuilder::Build(
                type,
                seed,
                [this, myToken, myCancel](const Maze& partial) {
                    if (myCancel->load(std::memory_order_relaxed)) return;
                    if (latestToken_.load(std::memory_order_relaxed) != myToken) return;

                    {
                        std::lock_guard<std::mutex> lk(latestMazeMutex_);
                        latestMaze_ = partial;
                        hasMaze_ = true;
                    }
                    setMaze(partial);
                },
                myCancel.get(),
                40,
                std::chrono::milliseconds(8)
            );

            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = myCancel->load(std::memory_order_relaxed) ? "Build cancelled." : "Build done.";
        }
        catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(uiMsgMutex_);
            uiLastMsg_ = std::string("Build exception: ") + e.what();
        }
    }).detach();
}

void MazeViewer::requestFindPath_(int sx, int sy, int ex, int ey)
{
    Maze snapshot{};
    {
        std::lock_guard<std::mutex> lk(latestMazeMutex_);
        if (!hasMaze_) {
            std::lock_guard<std::mutex> lk2(uiMsgMutex_);
            uiLastMsg_ = "No maze yet. Build first.";
            return;
        }
        snapshot = latestMaze_;
    }

    const uint64_t myToken = latestToken_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::shared_ptr<std::atomic<bool>> myCancel;
    {
        std::lock_guard<std::mutex> lk(cancelMutex_);
        cancelFlag_->store(true, std::memory_order_relaxed);
        cancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        myCancel = cancelFlag_;
    }

    const uint32_t updateEvery = (uiUpdateEvery_ <= 0) ? 1u : (uint32_t)uiUpdateEvery_;
    const auto delay = std::chrono::milliseconds(std::max(0, uiDelayMs_));

    {
        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        uiLastMsg_ = "Finding path...";
    }

    std::thread([this, snapshot, sx, sy, ex, ey, myToken, myCancel, updateEvery, delay] {
        std::vector<std::pair<int32_t, int32_t>> steps;
        std::string err;

        bool ok = PathFinder::FindPath(
            snapshot,
            {sx, sy},
            {ex, ey},
            steps,
            err,
            [this, myToken, myCancel](const Maze& animMaze) {
                if (myCancel->load(std::memory_order_relaxed)) return;
                if (latestToken_.load(std::memory_order_relaxed) != myToken) return;
                setMaze(animMaze);
            },
            myCancel.get(),
            updateEvery,
            delay
        );

        std::lock_guard<std::mutex> lk(uiMsgMutex_);
        if (myCancel->load(std::memory_order_relaxed)) uiLastMsg_ = "Path cancelled.";
        else uiLastMsg_ = ok ? "Path done." : ("Path failed: " + err);
    }).detach();
}
// ------------------------------------------------------------------------

void MazeViewer::run()
{
    initWindowAndGL_();

    while (!glfwWindowShouldClose(static_cast<GLFWwindow *>(window_)))
    {
        if (closeRequested_.load(std::memory_order_relaxed))
        {
            glfwSetWindowShouldClose(static_cast<GLFWwindow *>(window_), GLFW_TRUE);
        }

        processEvents_();
        renderFrame_();
    }

    shutdownGL_();
}