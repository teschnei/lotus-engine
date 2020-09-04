#include "worker_pool.h"
#include "core.h"
#include "work_item.h"
#include <algorithm>
#include <iostream>

namespace lotus
{
    WorkerPool::WorkerPool(Engine* _engine) : engine(_engine), main_thread(_engine, this)
    {
        for (size_t i = 0; i < std::thread::hardware_concurrency() - 1; ++i)
        {
            threads.push_back(std::make_unique<WorkerThreadConcurrent>(engine, this));
        }
        processing_work.resize(engine->renderer->getImageCount());
    }

    WorkerPool::~WorkerPool()
    {
        for(auto& thread : threads)
        {
            thread->Exit();
        }
        exit = true;
        work_cv.notify_all();

        for(auto& thread : threads)
        {
            thread->Join();
        }
    }

    void WorkerPool::addForegroundWork(std::unique_ptr<WorkItem> work_item)
    {
        std::lock_guard lg(work_mutex);
        work.push(std::move(work_item));
        work_cv.notify_one();
    }

    void WorkerPool::addBackgroundWork(std::unique_ptr<BackgroundWork> work_item)
    {
        std::lock_guard lg(work_mutex);
        background_work.push(std::move(work_item));
        work_cv.notify_one();
    }

    std::unique_ptr<WorkItem> WorkerPool::waitForWork()
    {
        std::unique_lock lk(work_mutex);
        if (work.empty() && background_work.empty() && !exit)
        {
            work_cv.wait(lk, [this] {return !work.empty() || !background_work.empty() || exit; });
        }
        if (!background_work.empty())
            return background_work.top_and_pop();
        else if (!work.empty())
            return work.top_and_pop();
        else
            return {};
    }

    void WorkerPool::workFinished(std::unique_ptr<WorkItem>&& work_item)
    {
        std::lock_guard lg(work_mutex);
        if (auto bg = dynamic_cast<BackgroundWork*>(work_item.get()))
        {
            pending_background_work.push_back(std::move(work_item));
        }
        else
        {
            pending_work.push_back(std::move(work_item));
            idle_cv.notify_all();
        }
    }

    std::vector<vk::CommandBuffer> WorkerPool::getPrimaryGraphicsBuffers(int image)
    {
        std::vector<vk::CommandBuffer> buffers;
        for (const auto& task : processing_work[image])
        {
            if (task->graphics.primary)
                buffers.push_back(task->graphics.primary);
        }
        return buffers;
    }

    std::vector<vk::CommandBuffer> WorkerPool::getSecondaryGraphicsBuffers(int image)
    {
        std::vector<vk::CommandBuffer> buffers;
        for (const auto& task : processing_work[image])
        {
            if (task->graphics.secondary)
                buffers.push_back(task->graphics.secondary);
        }
        return buffers;
    }

    std::vector<vk::CommandBuffer> WorkerPool::getShadowmapGraphicsBuffers(int image)
    {
        std::vector<vk::CommandBuffer> buffers;
        for (const auto& task : processing_work[image])
        {
            if (task->graphics.shadow)
                buffers.push_back(task->graphics.shadow);
        }
        return buffers;
    }

    std::vector<vk::CommandBuffer> WorkerPool::getParticleGraphicsBuffers(int image)
    {
        std::vector<vk::CommandBuffer> buffers;
        for (const auto& task : processing_work[image])
        {
            if (task->graphics.particle)
                buffers.push_back(task->graphics.particle);
        }
        return buffers;
    }

    std::vector<vk::CommandBuffer> WorkerPool::getPrimaryComputeBuffers(int image)
    {
        std::vector<vk::CommandBuffer> buffers;
        for (const auto& task : processing_work[image])
        {
            if (task->compute.primary)
                buffers.push_back(task->compute.primary);
        }
        return buffers;
    }

    void WorkerPool::clearProcessed(int image)
    {
        //a mutex is not needed here because the fence already assures us that we have nothing being posted to this queue yet
        std::swap(processing_work[image], finished_work);
    }

    void WorkerPool::deleteFinished()
    {
        finished_work.clear();
    }

    void WorkerPool::startProcessing(int image)
    {
        std::swap(pending_work, processing_work[image]);
        std::sort(processing_work[image].rbegin(), processing_work[image].rend(), WorkCompare());
    }

    void WorkerPool::waitIdle()
    {
        std::unique_lock lk(work_mutex);
        time_point wait_start = sim_clock::now();
        if (work.empty() && std::none_of(threads.begin(), threads.end(), [](const auto& thread) {return thread->Busy(); })) return;
        while (!work.empty())
        {
            auto new_work = work.top_and_pop();
            ProcessMainThread(lk, std::move(new_work));
        }
        idle_cv.wait(lk, [this]
        {
            return work.empty() && std::none_of(threads.begin(), threads.end(), [](const auto& thread)
            {
                return thread->Busy();
            });
        });
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(sim_clock::now() - wait_start);
        std::cout << "elapsed:" << elapsed.count() << std::endl;
    }

    void WorkerPool::reset()
    {
        waitIdle();
        work.clear();
        pending_work.clear();
        finished_work.clear();
        for (auto& work_vec : processing_work)
        {
            work_vec.clear();
        }
    }

    void WorkerPool::checkBackgroundWork()
    {
        for (const auto& work : pending_background_work)
        {
            static_cast<BackgroundWork*>(work.get())->Callback(engine);
        }
    }

    void WorkerPool::clearBackgroundWork(int image)
    {
        //move any finished background work into the processing work vector, where it will stay until the next time this image is used
        processing_work[image].insert(processing_work[image].end(), std::make_move_iterator(pending_background_work.begin()), std::make_move_iterator(pending_background_work.end()));
        pending_background_work.clear();
    }

    void WorkerPool::ProcessMainThread(std::unique_lock<std::mutex>& lock, std::unique_ptr<WorkItem>&& work_item)
    {
        lock.unlock();
        work_item->Process(&main_thread);
        workFinished(std::move(work_item));
        lock.lock();
    }
}
