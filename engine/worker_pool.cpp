#include "worker_pool.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    WorkerPool::WorkerPool(Engine* _engine) : engine(_engine)
    {
        temp_pool = this;
        worker_flag.test_and_set();
        finished_tasks.resize(engine->renderer->getFrameCount());
        for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
        {
            threads.emplace_back([this](std::stop_token stop)
            {
                auto thread_locals = engine->renderer->createThreadLocals();
                runTasks(stop);
            });
        }
    }

    void WorkerPool::Run()
    {
        auto thread_locals = engine->renderer->createThreadLocals();
        while (!threads[0].get_stop_token().stop_requested())
        {
            main_flag.clear();
            main_flag.notify_one();
            main_flag.wait(false);
            if (!threads[0].get_stop_token().stop_requested())
            {
                auto task = std::exchange(main_task, nullptr);
                task->awaiting.resume();
            }
        }
        if (exception) std::rethrow_exception(exception);
    }

    void WorkerPool::Stop()
    {
        for (auto& thread : threads)
        {
            thread.request_stop();
        }
        main_flag.test_and_set();
        main_flag.notify_one();
        ScheduledTask t(this);
        task_head.store(&t);
        task_head.notify_all();
    }

    void WorkerPool::Stop(std::exception_ptr ptr)
    {
        exception = ptr;
        Stop();
    }

    void WorkerPool::runTasks(std::stop_token stop)
    {
        task_head.wait(nullptr);
        while (!stop.stop_requested())
        {
            if (!worker_flag.test_and_set())
            {
                main_flag.wait(true);
            }
            if (!stop.stop_requested())
            {
                auto* task = tryGetTask();
                if (task)
                    task->awaiting.resume();
                task_head.wait(nullptr);
            }
        }
    }

    WorkerPool::ScheduledTask* WorkerPool::tryGetTask()
    {
        auto head = task_head.load();
        while (head && !task_head.compare_exchange_weak(head, head->next, std::memory_order::seq_cst, std::memory_order::relaxed));

        return head;
    }

    void WorkerPool::queueTask(ScheduledTask* task)
    {
        task->next = task_head.load(std::memory_order::relaxed);
        while (!task_head.compare_exchange_weak(task->next, task, std::memory_order::seq_cst, std::memory_order::relaxed));

        task_head.notify_one();
    }

    std::vector<vk::CommandBuffer> WorkerPool::getPrimaryGraphicsBuffers(int)
    {
        return command_buffers.graphics_primary.getAll();
    }
    std::vector<vk::CommandBuffer> WorkerPool::getSecondaryGraphicsBuffers(int)
    {
        return command_buffers.graphics_secondary.getAll();
    }
    std::vector<vk::CommandBuffer> WorkerPool::getShadowmapGraphicsBuffers(int)
    {
        return command_buffers.shadowmap.getAll();
    }
    std::vector<vk::CommandBuffer> WorkerPool::getParticleGraphicsBuffers(int)
    {
        return command_buffers.particle.getAll();
    }

    void WorkerPool::processFrameWaits()
    {
        for (auto& task : frame_waiting_queue.getAll())
        {
            task->awaiting.resume();
        }
    }

    void WorkerPool::beginProcessing(size_t image)
    {
        finished_tasks[image] = std::move(processing_tasks);
    }

    void WorkerPool::clearProcessed(size_t image)
    {
        deletion_tasks = std::move(finished_tasks[image]);
    }

    void WorkerPool::deleteFinished()
    {
        deletion_tasks = {};
    }

    void WorkerPool::Reset()
    {

    }
}
