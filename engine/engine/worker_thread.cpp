#include "worker_thread.h"
#include "worker_pool.h"
#include "core.h"

lotus::WorkerThread::WorkerThread(Engine* _engine, WorkerPool* _pool) : pool(_pool), engine(_engine)
{
    auto [graphics_queue, present_queue, compute_queue] = engine->renderer.getQueueFamilies(engine->renderer.physical_device);

    vk::CommandPoolCreateInfo pool_info = {};
    pool_info.queueFamilyIndex = graphics_queue.value();
    graphics.command_pool = engine->renderer.device->createCommandPoolUnique(pool_info);

    pool_info.queueFamilyIndex = compute_queue.value();
    compute.command_pool = engine->renderer.device->createCommandPoolUnique(pool_info);

    std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(engine->renderer.getImageCount());
    poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(engine->renderer.getImageCount());

    vk::DescriptorPoolCreateInfo poolInfo = {};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(engine->renderer.getImageCount());

    desc_pool = engine->renderer.device->createDescriptorPoolUnique(poolInfo);

    graphics.primary_buffers.resize(engine->renderer.getImageCount());
    graphics.secondary_buffers.resize(engine->renderer.getImageCount());
    graphics.shadow_buffers.resize(engine->renderer.getImageCount());

    compute.primary_buffers.resize(engine->renderer.getImageCount());
    compute.events.resize(engine->renderer.getImageCount());
}

void lotus::WorkerThread::WorkLoop()
{
    while (true)
    {
        pool->waitForWork(&work);
        work->Process(this);
        pool->workFinished(&work);
    }
}
