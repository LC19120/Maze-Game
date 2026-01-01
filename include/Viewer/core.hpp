#pragma once

#include "core/Common.hpp"
#include "core/DataStruct.hpp"
#include "core/PathFinder.hpp"

// UI 枚举，表示当前聚焦的输入框类型
enum class UI
{
    None,           // 无焦点
    Seed,           // 种子输入框
    BreakCount,     // 破墙次数输入框
    StartX,         // 起点 X 输入框
    StartY,         // 起点 Y 输入框
    EndX,           // 终点 X 输入框
    EndY,           // 终点 Y 输入框
    UpdateEvery,    // 更新频率输入框
    DelayMs         // 动画延迟输入框
};

// Viewer 类，负责迷宫的渲染、交互和主逻辑
class Viewer {
public:
    // 获取 Viewer 单例对象
    static Viewer& getInstance();
    // 主循环，初始化窗口和 OpenGL，渲染迷宫和 UI
    void run();
    // 处理窗口尺寸变化，更新帧缓冲区宽高
    void onFramebufferResized(int width, int height);

private:
    // 构造函数
    Viewer();
    // 析构函数，关闭 OpenGL
    ~Viewer();

private:
    // -------- 窗口和 OpenGL 相关 --------
    // 初始化窗口和 OpenGL
    void initWindowAndGL();
    // 关闭 OpenGL
    void shutdownGL();
    // 初始化 UI 回调
    void initUiCallbacks();
    // 更新窗口标题
    void updateWindowTitle();

    // 应用输入框编辑内容
    void applyEdit();

    // -------- 渲染相关 --------
    // 绘制迷宫
    void drawMaze();
    // 绘制 UI 面板
    void renderUi();
    // 根据迷宫数据重建渲染网格
    void rebuildMeshFromMaze(const Maze& m);
    // 如果迷宫数据有变则重建渲染网格
    void rebuildMesh();

    // -------- 迷宫和路径相关 --------
    // 生成迷宫，并同步 UI 起终点
    void buildMaze(int32_t seed);
    // 寻找路径或计数路径，根据算法类型更新动画和迷宫状态
    void findPath(int32_t sx, int32_t sy, int32_t ex, int32_t ey, int32_t algoIndex);
    // 更新路径动画（根据动画模式刷新迷宫显示）
    void pathAnim();
    // PathPasser 按钮功能：强制路径经过指定点
    void passPath(int32_t x, int32_t y);

    // 路径动画相关数据结构
    struct PathAnim
    {
        bool active = false; // 是否正在播放动画
        std::chrono::steady_clock::time_point t0{}; // 动画起始时间

        int mode = 0; // 0: 路径/破墙动画，1: 计数覆盖动画

        // 路径/破墙动画数据
        std::vector<Point> visited; // 已访问点
        std::vector<Point> path;    // 路径点
        int visitedVal = 15;        // 已访问点的颜色值
        int pathVal    = 5;         // 路径点的颜色值

        bool hasOrigWall = false;   // 是否保存了原始墙体
        std::vector<uint8_t> origWall; // 原始墙体数据

        size_t lastVisitedN = (size_t)-1; // 上一次已访问点数量
        size_t lastPathN    = (size_t)-1; // 上一次路径点数量

        // 计数动画数据
        std::vector<std::vector<Point>> allPaths; // 所有路径
        int totalPaths = 0;                       // 路径总数
        std::vector<size_t> lastLenPerPath;       // 每条路径的长度
        std::vector<int32_t> passCount;           // 每条路径经过次数
    } anim;

    bool alphaOverrideActive = false;         // 是否启用透明度覆盖
    std::vector<float> cellAlphaOverride;     // 单元格透明度覆盖

private:
    // -------- 窗口 / OpenGL 状态 --------
    void* window = nullptr;                   // 窗口指针
    int fbW = 900;                            // 帧缓冲区宽度
    int fbH = 600;                            // 帧缓冲区高度

    uint32_t program = 0;                     // 着色器程序
    uint32_t vao = 0;                         // 顶点数组对象
    uint32_t vbo = 0;                         // 顶点缓冲对象
    uint32_t uiVao = 0;                       // UI 顶点数组对象
    uint32_t uiVbo = 0;                       // UI 顶点缓冲对象

    int vertexCount = 0;                      // 迷宫顶点数量
    int uiVertexCount = 0;                    // UI 顶点数量

    int meshBuiltFbW = 0;                     // 网格构建时的宽度
    int meshBuiltFbH = 0;                     // 网格构建时的高度

    // -------- 迷宫状态（与渲染器共享） --------
    Maze maze{};                              // 当前迷宫
    bool mazeLoaded = false;                  // 是否已加载迷宫
    bool mazeDirty = false;                   // 迷宫数据是否有变

    Maze latestMaze{};                        // 最近一次生成的迷宫
    bool hasMaze = false;                     // 是否有迷宫

    // -------- UI 状态 --------
    UI uiFocus = UI::None;                    // 当前聚焦的输入框
    std::string uiEdit;                       // 输入框编辑内容
    int uiSeed = 0;                           // 当前种子
    int uiBreakCount = 1;                     // 破墙次数
    int uiStartX = 1, uiStartY = 1;           // 起点坐标
    int uiEndX = 1, uiEndY = 1;               // 终点坐标
    int uiUpdateEvery = 4;                    // 更新频率
    int uiDelayMs = 0;                        // 动画延迟
    int uiAlgoIndex = 0;                      // 当前算法类型

    // “路径信息”框中显示的结果
    int lastPathLen  = 0;   // 路径长度（A*）
    int lastBreakLen = 0;   // 破墙路径长度
    int lastCountWays = 0;  // 路径总数（计数模式）
    int lastPassLen  = 0;   // 强制经过点的路径长度

    std::vector<uint8_t> baseWall;            // 基础墙体数据（1=墙，0=空）
};