#pragma once

#include "worker_thread.h"
#include "worker_thread_concurrent.h"
#include "work_item.h"
#include "background_work.h"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace lotus
{
    class Engine;
    template <
        class T,
        class Container = std::vector<T>,
        class Compare = std::less<typename Container::value_type>>
    class WorkItemQueue : public std::priority_queue<T, Container, Compare> {
    public:
        T top_and_pop()
        {
            std::pop_heap(c.begin(), c.end(), comp);
            T value = std::move(c.back());
            c.pop_back();
            return value;
        }

        void clear()
        {
            c.clear();
        }

        typename Container::iterator begin() noexcept
        {
            return c.begin();
        }

        typename Container::iterator end() noexcept
        {
            return c.end();
        }

    protected:
        using std::priority_queue<T, Container, Compare>::c;
        using std::priority_queue<T, Container, Compare>::comp;
    };

    class WorkItem;

    class WorkCompare
    {
    public:
        bool operator() (std::unique_ptr<WorkItem>& a, std::unique_ptr<WorkItem>& b) const
        {
            return *a > *b;
        }
        bool operator() (std::unique_ptr<BackgroundWork>& a, std::unique_ptr<BackgroundWork>& b) const
        {
            return *a > *b;
        }
    };

    class WorkerPool
    {
    public:
        WorkerPool(Engine*);
        ~WorkerPool();

        void addForegroundWork(std::unique_ptr<WorkItem>);
        template<typename Container>
        void addForegroundWork(Container& c)
        {
            std::lock_guard lg(work_mutex);
            for (auto& item : c)
            {
                work.push(std::move(item));
            }
            work_cv.notify_one();
        }
        void addBackgroundWork(std::unique_ptr<BackgroundWork>);
        template<typename Container>
        void addBackgroundWork(Container& c)
        {
            std::lock_guard lg(work_mutex);
            for (auto& item : c)
            {
                background_work.push(std::move(item));
            }
            work_cv.notify_one();
        }
        std::unique_ptr<WorkItem> waitForWork();
        void workFinished(std::unique_ptr<WorkItem>&&);
        std::vector<vk::CommandBuffer> getPrimaryGraphicsBuffers(int image);
        std::vector<vk::CommandBuffer> getSecondaryGraphicsBuffers(int image);
        std::vector<vk::CommandBuffer> getShadowmapGraphicsBuffers(int image);
        std::vector<vk::CommandBuffer> getParticleGraphicsBuffers(int image);

        std::vector<vk::CommandBuffer> getPrimaryComputeBuffers(int image);

        void clearProcessed(int image);
        void deleteFinished();
        void startProcessing(int image);
        void waitIdle();
        void reset();
        void checkBackgroundWork();
        void clearBackgroundWork(int image);

    private:
        void ProcessMainThread(std::unique_lock<std::mutex>& lock, std::unique_ptr<WorkItem>&& work);

        std::vector<std::unique_ptr<WorkerThreadConcurrent>> threads;
        WorkItemQueue<std::unique_ptr<WorkItem>, std::vector<std::unique_ptr<WorkItem>>, WorkCompare> work;
        std::vector<std::unique_ptr<WorkItem>> pending_work;
        std::vector<std::vector<std::unique_ptr<WorkItem>>> processing_work;
        std::vector<std::unique_ptr<WorkItem>> finished_work;
        WorkItemQueue<std::unique_ptr<BackgroundWork>, std::vector<std::unique_ptr<BackgroundWork>>, WorkCompare> background_work;
        std::vector<std::unique_ptr<WorkItem>> pending_background_work;
        std::mutex work_mutex;
        std::condition_variable work_cv;
        std::condition_variable idle_cv;
        bool exit{ false };

        Engine* engine {nullptr};
        WorkerThread main_thread;
    };
}
