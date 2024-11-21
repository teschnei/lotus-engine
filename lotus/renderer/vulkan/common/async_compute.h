#pragma once

#include "lotus/async_queue.h"
#include "lotus/task.h"
#include "lotus/worker_pool.h"

import vulkan_hpp;

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
} // namespace lotus
