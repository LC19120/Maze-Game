#include "core/Common.hpp"
#include "Thread/ThreadPool.hpp"

// Initialize the thread pool
ThreadPool::ThreadPool(size_t threadCount)
{
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4;
    }

    workers_.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

size_t ThreadPool::size() const noexcept
{
    return workers_.size();
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stop_) return;
        stop_ = true;
    }
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();

    {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<std::function<void()>> empty;
        tasks_.swap(empty);
    }
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&] { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) return;

            job = std::move(tasks_.front());
            tasks_.pop();
        }
        job();
    }
}