#include "texture_init.h"
#include <utility>
#include "../worker_thread.h"
#include "../core.h"

namespace lotus
{
    TextureInitTask::TextureInitTask(int _image_index, std::shared_ptr<Texture> _texture, vk::Format _format, vk::ImageTiling _tiling, std::vector<uint8_t>&& _texture_data) :
        WorkItem(), image_index(_image_index), texture(std::move(_texture)), format(_format), tiling(_tiling), texture_data(std::move(_texture_data))
    {
        priority = -2;
    }

    void TextureInitTask::Process(WorkerThread* thread)
    {
        staging_buffer = thread->engine->renderer->gpu->memory_manager->GetBuffer(texture_data.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = staging_buffer->map(0, texture_data.size(), {});
        memcpy(data, texture_data.data(), texture_data.size());
        staging_buffer->unmap();

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::ImageMemoryBarrier barrier;
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image->image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = vk::Extent3D{
            texture->getWidth(),
            texture->getHeight(),
            1
        };
        command_buffer->copyBufferToImage(staging_buffer->buffer, texture->image->image, vk::ImageLayout::eTransferDstOptimal, region);

        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image->image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);

        command_buffer->end();

        graphics.primary = *command_buffer;

    }
}
