#include "renderer_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/hybrid/renderer_hybrid.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"
#include "engine/renderer/vulkan/raytrace/renderer_raytrace.h"

namespace lotus
{
    RendererHybridInitTask::RendererHybridInitTask(RendererHybrid* _renderer) : WorkItem(), renderer(_renderer)
    {
        priority = 0;
    }

    void RendererHybridInitTask::Process(WorkerThread* thread)
    {
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
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = renderer->rtx_gbuffer.light.image->image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, nullptr, nullptr, barrier);

        command_buffer->end();

        graphics.primary = *command_buffer;
    }

    RendererRasterizationInitTask::RendererRasterizationInitTask(RendererRasterization* _renderer) : WorkItem(), renderer(_renderer)
    {
        priority = 0;
    }

    void RendererRasterizationInitTask::Process(WorkerThread* thread)
    {

    }

    RendererRaytraceInitTask::RendererRaytraceInitTask(RendererRaytrace* _renderer) : WorkItem(), renderer(_renderer)
    {
        priority = 0;
    }

    void RendererRaytraceInitTask::Process(WorkerThread* thread)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::ImageMemoryBarrier barrier_albedo;
        barrier_albedo.oldLayout = vk::ImageLayout::eUndefined;
        barrier_albedo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier_albedo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_albedo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_albedo.image = renderer->rtx_gbuffer.albedo.image->image;
        barrier_albedo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier_albedo.subresourceRange.baseMipLevel = 0;
        barrier_albedo.subresourceRange.levelCount = 1;
        barrier_albedo.subresourceRange.baseArrayLayer = 0;
        barrier_albedo.subresourceRange.layerCount = 1;
        barrier_albedo.srcAccessMask = {};
        barrier_albedo.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        vk::ImageMemoryBarrier barrier_light = barrier_albedo;
        barrier_light.image = renderer->rtx_gbuffer.light.image->image;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, nullptr, nullptr, {barrier_albedo, barrier_light});

        command_buffer->end();

        graphics.primary = *command_buffer;
    }
}
