#pragma once
#include "core/Common.hpp"


class ThreadPool final {
public:
    explicit ThreadPool(size_t threadCount = 4);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    size_t size() const noexcept;

    void shutdown();

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<decltype(f(std::declval<Args>()...))>
    {
        using R = decltype(f(std::declval<Args>()...));

        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

private:
    void workerLoop();

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_{false};

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
};