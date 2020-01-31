#include "acceleration_build.h"
#include "engine/worker_thread.h"
#include "engine/core.h"

namespace lotus
{
    AccelerationBuildTask::AccelerationBuildTask(int _image_index, const std::shared_ptr<TopLevelAccelerationStructure>& _as)
        : WorkItem(), image_index(_image_index), as(_as)
    {
        priority = 2;
    }

    void AccelerationBuildTask::Process(WorkerThread* thread)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        as->Build(*command_buffer);

        command_buffer->end(thread->engine->renderer.dispatch);

        compute.primary = *command_buffer;
    }
}
