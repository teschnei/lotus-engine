#include "model_init.h"
#include <utility>
#include "../worker_thread.h"
#include "../core.h"

namespace lotus
{
    ModelInitTask::ModelInitTask(int _image_index, std::shared_ptr<Mesh> _model, std::vector<uint8_t>&& _vertex_buffer,
        std::vector<uint8_t>&& _index_buffer) : WorkItem(), image_index(_image_index), model(std::move(_model)), vertex_buffer(std::move(_vertex_buffer)), index_buffer(std::move(_index_buffer))
    {
        priority = -1;
    }

    void ModelInitTask::Process(WorkerThread* thread)
    {
        vk::DeviceSize buffer_size = vertex_buffer.size() + index_buffer.size();

        staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = thread->engine->renderer.device->mapMemory(staging_buffer->memory, staging_buffer->memory_offset, buffer_size, {}, thread->engine->renderer.dispatch);
        memcpy(data, vertex_buffer.data(), vertex_buffer.size());
        memcpy(static_cast<uint8_t*>(data) + vertex_buffer.size(), index_buffer.data(), index_buffer.size());
        thread->engine->renderer.device->unmapMemory(staging_buffer->memory, thread->engine->renderer.dispatch);

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->command_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        std::array<vk::BufferMemoryBarrier, 2> barriers;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = *model->vertex_buffer->buffer;
        barriers[0].size = VK_WHOLE_SIZE;
        barriers[0].srcAccessMask = {};
        barriers[0].dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].buffer = *model->index_buffer->buffer;
        barriers[1].size = VK_WHOLE_SIZE;
        barriers[1].srcAccessMask = {};
        barriers[1].dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, barriers, nullptr, thread->engine->renderer.dispatch);

        vk::BufferCopy copy_region = {};
        copy_region.size = vertex_buffer.size();
        command_buffer->copyBuffer(*staging_buffer->buffer, *model->vertex_buffer->buffer, copy_region, thread->engine->renderer.dispatch);
        copy_region.size = index_buffer.size();
        copy_region.srcOffset = vertex_buffer.size();
        command_buffer->copyBuffer(*staging_buffer->buffer, *model->index_buffer->buffer, copy_region, thread->engine->renderer.dispatch);

        command_buffer->end(thread->engine->renderer.dispatch);

        thread->primary_buffers[image_index].push_back(*command_buffer);
    }
}
