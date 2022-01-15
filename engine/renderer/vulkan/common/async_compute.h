#pragma once

#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/worker_pool.h"
#include "engine/async_queue.h"
#include "engine/task.h"

namespace lotus
{
    class Engine;
    class Renderer;

    class AsyncCompute
    {
    public:
        AsyncCompute(Engine*, Renderer*);
        Task<> compute(vk::UniqueCommandBuffer buffer);
    private:
        Task<> queue_compute(vk::UniqueCommandBuffer buffer);
        void checkTasks();
        Engine* engine;
        Renderer* renderer;
        vk::UniqueFence fence;
        struct QueueItem
        {
            vk::UniqueCommandBuffer buffer;
            std::unique_ptr<WorkerPool::ScheduledTask> scheduled_task;
        };
        AsyncQueue<QueueItem> tasks;
        std::atomic<uint64_t> task_count;
    };
}