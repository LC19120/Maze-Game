#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "core/MazeViewer.hpp"
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

    if (t == "small" || t == "s")   { out = MazeType::Small;  return true; }
    if (t == "medium" || t == "m")  { out = MazeType::Medium; return true; }
    if (t == "large" || t == "l")   { out = MazeType::Large;  return true; }
    if (t == "ultra" || t == "u")   { out = MazeType::Ultra;  return true; }

    return false;
}

void runApp()
{
    auto& viewer = MazeViewer::getInstance();
    viewer.run();
}