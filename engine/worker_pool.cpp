#include "worker_pool.h"
#include "core.h"
#include "work_item.h"
#include <algorithm>
#include <iostream>
#include <ranges>

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

    void WorkerPool::addForegroundWork(UniqueWork work_item)
    {
        std::scoped_lock lg(work_mutex);
        work.push(std::move(work_item));
        work_cv.notify_one();
    }

    UniqueWork WorkerPool::waitForWork()
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

    void WorkerPool::workFinished(UniqueWork&& work_item)
    {
        std::scoped_lock lg(work_mutex);
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
        std::ranges::sort(processing_work[image], WorkCompare());
    }

    void WorkerPool::waitIdle()
    {
        std::unique_lock lk(work_mutex);
        if (work.empty() && std::ranges::none_of(threads, [](const auto& thread) {return thread->Busy(); })) return;
        while (!work.empty())
        {
            auto new_work = work.top_and_pop();
            ProcessMainThread(lk, std::move(new_work));
        }
        idle_cv.wait(lk, [this]
        {
            return work.empty() && std::ranges::none_of(threads, [](const auto& thread)
            {
                return thread->Busy();
            });
        });
    }

    void WorkerPool::reset()
    {
        waitIdle();
        work.clear();
        pending_work.clear();
        pending_background_work.clear();
        finished_work.clear();
        for (auto& work_vec : processing_work)
        {
            work_vec.clear();
        }
    }

    void WorkerPool::checkBackgroundWork()
    {
        {
            std::scoped_lock lg(work_mutex);
            for (const auto& work : pending_background_work)
            {
                if (auto bg = static_cast<BackgroundWork*>(work.get()); bg->Processed())
                {
                    bg->Callback(engine);
                }
            }
        }
        {
            std::scoped_lock lg(group_mutex);
            std::erase_if(workgroups, [](auto& group)
            {
                return group->Finished();
            });
        }
    }

    void WorkerPool::clearBackgroundWork(int image)
    {
        std::scoped_lock lk(work_mutex);
        //move any finished background work into the processing work vector, where it will stay until the next time this image is used
        pending_background_work.erase(std::remove_if(pending_background_work.begin(), pending_background_work.end(), [this, image](auto& work)
        {
            if (auto bg_work = static_cast<BackgroundWork*>(work.get()); bg_work->Finished())
            {
                processing_work[image].insert(processing_work[image].end(), std::move(bg_work->GetWork()));
                return true;
            }
            return false;
        }), pending_background_work.end());
    }

    void WorkerPool::ProcessMainThread(std::unique_lock<std::mutex>& lock, UniqueWork&& work_item)
    {
        lock.unlock();
        work_item->Process(&main_thread);
        workFinished(std::move(work_item));
        lock.lock();
    }
}
