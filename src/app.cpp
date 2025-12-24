#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "core/MazeViewer.hpp"
#include "Thread/ThreadPool.hpp"


static bool parseMazeType(const std::string& s, MazeType& out)
{
    if (s == "Small")  { out = MazeType::Small;  return true; }
    if (s == "Medium") { out = MazeType::Medium; return true; }
    if (s == "Large")  { out = MazeType::Large;  return true; }
    if (s == "Ultra")  { out = MazeType::Ultra;  return true; }
    return false;
}

void runApp()
{
    auto& viewer = MazeViewer::getInstance();

    ThreadPool pool(std::max(1u, std::thread::hardware_concurrency() > 2
                                    ? std::thread::hardware_concurrency() - 2
                                    : 1u));

    std::atomic<uint64_t> latestToken{0};

    std::mutex cancelMutex;
    std::shared_ptr<std::atomic<bool>> cancelFlag = std::make_shared<std::atomic<bool>>(false);

    std::thread inputThread([&] {
        for (;;)
        {
            std::cout << "\n[b] build  [q] quit\n> " << std::flush;

            std::string cmd;
            if (!(std::cin >> cmd)) break;

            if (cmd == "q") {
                viewer.requestClose();
                break;
            }

            if (cmd != "b") {
                std::cout << "Invalid.\n";
                continue;
            }

            int32_t seed = 0;
            std::cout << "seed: " << std::flush;
            if (!(std::cin >> seed)) break;

            std::cout << "type (Small/Medium/Large/Ultra): " << std::flush;
            std::string typeStr;
            if (!(std::cin >> typeStr)) break;

            MazeType type{};
            if (!parseMazeType(typeStr, type)) {
                std::cout << "Invalid type.\n";
                continue;
            }

            const uint64_t myToken = latestToken.fetch_add(1, std::memory_order_relaxed) + 1;

            std::shared_ptr<std::atomic<bool>> myCancel;
            {
                std::lock_guard<std::mutex> lk(cancelMutex);
                cancelFlag->store(true, std::memory_order_relaxed);
                cancelFlag = std::make_shared<std::atomic<bool>>(false);
                myCancel = cancelFlag;
            }

            pool.enqueue([&, seed, type, myToken, myCancel] {
                MazeBuilder::Build(
                    type,
                    seed,
                    [&](const Maze& partial) {
                        if (myCancel->load(std::memory_order_relaxed)) return;
                        if (latestToken.load(std::memory_order_relaxed) != myToken) return;
                        viewer.setMaze(partial);
                    },
                    myCancel.get(),
                    40,
                    std::chrono::milliseconds(8)
                );
            });
        }
    });

    viewer.run();

    if (inputThread.joinable()) inputThread.join();
    pool.shutdown();
}