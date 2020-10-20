#include "worker_pool.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    WorkerPool::WorkerPool(Engine* _engine) : engine(_engine)
    {
        temp_pool = this;
        worker_flag.test_and_set();
        finished_tasks.resize(engine->renderer->getImageCount());
        //for (size_t i = 0; i < 1/*std::thread::hardware_concurrency()*/; ++i)
        {
            threads.emplace_back([this](std::stop_token stop)
            {
                engine->renderer->createThreadLocals();
                runTasks(stop);
            });
        }
    }

    void WorkerPool::Wait(WorkerTask<> mainLoop)
    {
        //the threads all destruct together, and jthreads join in destruction
        auto task = std::move(mainLoop);
        //threads[0].join();
        engine->renderer->createThreadLocals();
        main_flag.wait(false);
        while (!threads[0].get_stop_token().stop_requested())
        {
            auto task = std::exchange(main_task, nullptr);
            task->awaiting.resume();
            main_flag.clear();
            main_flag.notify_one();
            main_flag.wait(false);
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

    WorkerPool::ScheduledTask* WorkerPool::getTask()
    {
        ScheduledTask* task = nullptr;
        while (!task)
        {
            task_head.wait(nullptr);
            task = tryGetTask();
        }
        return task;
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
            auto* task = tryGetTask();
            if (task)
                task->awaiting.resume();
            task_head.wait(nullptr);
        }
    }

    WorkerPool::ScheduledTask* WorkerPool::tryGetTask()
    {
        ScheduledTask* head = nullptr;
        do
        {
            head = task_head.load();
        } while (head && !task_head.compare_exchange_weak(head, head->next));

        return head;
    }

    void WorkerPool::queueTask(ScheduledTask* task)
    {
        ScheduledTask* head = nullptr;
        do
        {
            head = task_head.load();
            task->next = head;
        } while (!task_head.compare_exchange_weak(head, task));

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
    std::vector<vk::CommandBuffer> WorkerPool::getPrimaryComputeBuffers(int)
    {
        return command_buffers.compute.getAll();
    }
}
