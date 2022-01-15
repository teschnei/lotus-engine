#include "texture.h"
#include "engine/core.h"
#include "engine/worker_pool.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    WorkerTask<> Texture::Init(Engine* engine, std::vector<std::vector<uint8_t>>&& texture_datas)
    {
        size_t total_size = 0;
        for (const auto& texture_data : texture_datas)
        {
            total_size += texture_data.size();
        }
        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(total_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = staging_buffer->map(0, total_size, {});
        size_t offset = 0;
        for (const auto& texture_data : texture_datas)
        {
            memcpy((uint8_t*)data + offset, texture_data.data(), texture_data.size());
            offset += texture_data.size();
        }
        staging_buffer->unmap();

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::ImageMemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eNone,
            .srcAccessMask = vk::AccessFlagBits2KHR::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image->image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = static_cast<uint32_t>(texture_datas.size()),
            }
        };

        command_buffer->pipelineBarrier2KHR({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        });

        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = texture_datas.size();
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = vk::Extent3D{
            getWidth(),
            getHeight(),
            1
        };
        command_buffer->copyBufferToImage(staging_buffer->buffer, image->image, vk::ImageLayout::eTransferDstOptimal, region);

        barrier = {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
          //  .dstStageMask = vk::PipelineStageFlagBits2KHR::eFragmentShader,
           // .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image->image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = static_cast<uint32_t>(texture_datas.size())
            }
        };

        command_buffer->pipelineBarrier2KHR({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        });

        command_buffer->end();

        co_await engine->renderer->async_compute->compute(std::move(command_buffer));
        co_return;
    }
}
