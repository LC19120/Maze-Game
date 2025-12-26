#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "Viewer/core.hpp"
#include "core/PathFinder.hpp"
#include "Thread/ThreadPool.hpp"

#include <algorithm>
#include <cctype>
#include <string>

static bool parseMazeType(const std::string& s, MazeType& out)
{
    std::string t;
    t.reserve(s.size());
    for (unsigned char ch : s) t.push_back((char)std::tolower(ch));

    // only Medium exists now
    if (t == "medium" || t == "m" || t == "71") { out = MazeType::Medium; return true; }
    return false;
}

void runApp()
{
    auto& viewer = MazeViewer::getInstance();
    viewer.run();
}