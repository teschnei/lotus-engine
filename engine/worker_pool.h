#pragma once

#include "worker_thread.h"
#include "worker_thread_concurrent.h"
#include "work_item.h"
#include "workgroup.h"
#include "background_work.h"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <ranges>

namespace lotus
{
    class Engine;
    template <
        class T,
        class Container = std::vector<T>,
        class Compare = std::less<typename Container::value_type>>
    class WorkItemQueue : public std::priority_queue<T, Container, Compare>
    {
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

        using iterator = typename Container::iterator;

        iterator begin() noexcept
        {
            return c.begin();
        }

        iterator end() noexcept
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
        bool operator() (UniqueWork& a, UniqueWork& b) const
        {
            return *a > *b;
        }
        bool operator() (UniqueBackgroundWork& a, UniqueBackgroundWork& b) const
        {
            return *a > *b;
        }
    };

    class WorkerPool
    {
    public:
        WorkerPool(Engine*);
        ~WorkerPool();

        UniqueWork waitForWork();
        void workFinished(UniqueWork&&);
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
        void ProcessMainThread(std::unique_lock<std::mutex>& lock, UniqueWork&& work);

        std::vector<std::unique_ptr<WorkerThreadConcurrent>> threads;
        WorkItemQueue<UniqueWork, std::vector<UniqueWork>, WorkCompare> work;
        std::vector<UniqueWork> pending_work;
        std::vector<std::vector<UniqueWork>> processing_work;
        std::vector<UniqueWork> finished_work;
        WorkItemQueue<UniqueBackgroundWork, std::vector<UniqueBackgroundWork>, WorkCompare> background_work;
        std::vector<UniqueWork> pending_background_work;
        std::vector<std::unique_ptr<WorkGroup>> workgroups;
        std::mutex work_mutex;
        std::mutex group_mutex;
        std::condition_variable work_cv;
        std::condition_variable idle_cv;
        bool exit{ false };

        Engine* engine {nullptr};
        WorkerThread main_thread;
    public:

        void addForegroundWork(UniqueWork&&);

        void addForegroundWork(std::ranges::input_range auto&& r)
            requires std::indirectly_movable<std::ranges::iterator_t<decltype(r)>, typename decltype(work)::iterator>
        {
            std::scoped_lock lg(work_mutex);
            for (auto&& item : r)
            {
                work.push(std::move(item));
            }
            work_cv.notify_all();
        }

        template<typename Callback>
        void addBackgroundWork(UniqueWork&& work_item, Callback cb)
        {
            std::scoped_lock lg(work_mutex);
            background_work.emplace(std::move(work_item), cb);
            work_cv.notify_one();
        }

        void addBackgroundWork(std::ranges::input_range auto&& r)
            requires std::indirectly_movable<std::ranges::iterator_t<decltype(r)>, typename decltype(background_work)::iterator>
        {
            std::scoped_lock lg(work_mutex);
            for (auto&& item : r)
            {
                background_work.push(std::move(item));
            }
            work_cv.notify_all();
        }

        template<typename Container, typename Callback>
        void addBackgroundWork(Container&& c, Callback cb)
        {
            std::unique_lock lg(group_mutex);
            auto work = std::make_unique<WorkGroup>(c, cb);
            auto wg = work.get();
            workgroups.push_back(std::move(work));
            lg.unlock();
            wg->QueueItems(this);
        }
    };
}
