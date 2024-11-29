module;

#include <atomic>
#include <memory>

export module lotus:renderer.vulkan.common.async_compute;

import :util;
import vulkan_hpp;

export namespace lotus
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
