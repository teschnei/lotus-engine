#include "worker_pool.h"
#include "core.h"
#include "work_item.h"
#include <algorithm>

namespace lotus
{
    WorkerPool::WorkerPool(Engine* _engine) : engine(_engine)
    {
#ifdef SINGLETHREAD
        threads.push_back(std::make_unique<WorkerThread>(engine, this));
#else
        for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
        {
            threads.push_back(std::make_unique<WorkerThread>(engine, this));
        }
#endif
        processing_work.resize(engine->renderer.getImageCount());
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

    void WorkerPool::addWork(std::unique_ptr<WorkItem> work_item)
    {
        std::lock_guard lg(work_mutex);
        work.push(std::move(work_item));
        work_cv.notify_one();
    }

    void WorkerPool::waitForWork(std::unique_ptr<WorkItem>* item)
    {
        std::unique_lock lk(work_mutex);
        if (work.empty() && !exit)
        {
            work_cv.wait(lk, [this] {return !work.empty() || exit; });
        }
        if (!work.empty())
            *item = work.top_and_pop();
        else
            *item = nullptr;
    }

    void WorkerPool::workFinished(std::unique_ptr<WorkItem>* work_item)
    {
        std::lock_guard lg(work_mutex);
        pending_work.push_back(std::move(*work_item));
        idle_cv.notify_all();
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
#ifdef SINGLETHREAD
        while (!work.empty())
        {
            auto work_item = waitForWork();
            work_item->Process(threads[0].get());
            workFinished(std::move(work_item));
        }
#else
        std::unique_lock lk(work_mutex);
        if (work.empty() && std::none_of(threads.begin(), threads.end(), [](const auto& thread) {return thread->Busy(); })) return;
        idle_cv.wait(lk, [this]
        {
            return work.empty() && std::none_of(threads.begin(), threads.end(), [](const auto& thread)
            {
                return thread->Busy();
            });
        });
#endif
    }
}
