#include "async_compute.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    AsyncCompute::AsyncCompute(Engine* _engine, Renderer* _renderer) : engine(_engine), renderer(_renderer)
    {
        fence = renderer->gpu->device->createFenceUnique({});
    }

    Task<> AsyncCompute::compute(vk::UniqueCommandBuffer buffer)
    {
        auto t = queue_compute(std::move(buffer));
        if (task_count.fetch_add(1) == 0)
        {
            checkTasks();
        }

        co_await t;
    }

    Task<> AsyncCompute::queue_compute(vk::UniqueCommandBuffer buffer)
    {
        engine->worker_pool->gpuResource(co_await tasks.wait({ .buffer = std::move(buffer) }));
    }

    void AsyncCompute::checkTasks()
    {
        //TODO: just put a mutex in here, it's not going to work lockless
        auto local_task_count = 1;
        while (local_task_count > 0)
        {
            auto pending_tasks = tasks.getAll();
            std::vector<vk::CommandBufferSubmitInfoKHR> submits;
            submits.resize(pending_tasks.size());
            std::ranges::transform(pending_tasks, submits.begin(), [](auto& i) { return vk::CommandBufferSubmitInfoKHR{ .commandBuffer = *i->data.buffer }; });
            renderer->gpu->async_compute_queue.submit2KHR({
                vk::SubmitInfo2KHR {
                    .commandBufferInfoCount = static_cast<uint32_t>(submits.size()),
                    .pCommandBufferInfos = submits.data(),
                }
            }, *fence);
            renderer->gpu->device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max());
            renderer->gpu->device->resetFences(*fence);

            for (auto& t : pending_tasks | std::ranges::views::take(pending_tasks.size() - 1))
            {
                t->data.scheduled_task = std::make_unique<WorkerPool::ScheduledTask>(engine->worker_pool.get(), t->awaiting);
                t->data.scheduled_task->queueTask();
            }
            pending_tasks.back()->awaiting.resume();

            local_task_count = task_count.fetch_sub(pending_tasks.size()) - pending_tasks.size();
        }
    }
}