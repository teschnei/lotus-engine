#include "landscape_entity_init.h"

#include "worker_thread.h"
#include "core.h"
#include "renderer/renderer.h"

namespace lotus
{
    LandscapeEntityInitTask::LandscapeEntityInitTask(const std::shared_ptr<LandscapeEntity>& _entity, std::vector<LandscapeEntity::InstanceInfo>&& _instance_info) :
        WorkItem(), entity(_entity), instance_info(_instance_info)
    {
    }

    void LandscapeEntityInitTask::Process(WorkerThread* thread)
    {
        for (int i = 0; i < thread->engine->renderer.getImageCount(); ++i)
        {
            entity->uniform_buffers.push_back(thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
        }

        populateInstanceBuffer(thread);

        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->command_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        entity->blended_buffers.resize(thread->engine->renderer.getImageCount());

        for (int i = 0; i < entity->command_buffers.size(); ++i)
        {
            auto& command_buffer = entity->command_buffers[i];
            vk::CommandBufferInheritanceInfo inheritInfo = {};
            inheritInfo.renderPass = *thread->engine->renderer.render_pass;
            inheritInfo.framebuffer = *thread->engine->renderer.frame_buffers[i];

            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            beginInfo.pInheritanceInfo = &inheritInfo;

            command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_graphics_pipeline, thread->engine->renderer.dispatch);

            drawModel(thread, *command_buffer, *entity->uniform_buffers[i]->buffer, false);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_graphics_pipeline, thread->engine->renderer.dispatch);

            drawModel(thread, *command_buffer, *entity->uniform_buffers[i]->buffer, true);

            command_buffer->end(thread->engine->renderer.dispatch);
        }
    }

    void LandscapeEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, vk::Buffer uniform_buffer, bool transparency)
    {
        for (const auto& model : entity->models)
        {
            auto [offset, count] = entity->instance_offsets[model->name];
            if (count > 0 && !model->meshes.empty())
            {
                command_buffer.bindVertexBuffers(1, *entity->instance_buffer->buffer, offset * sizeof(LandscapeEntity::InstanceInfo), thread->engine->renderer.dispatch);
                for (const auto& mesh : model->meshes)
                {
                    if (mesh->has_transparency == transparency)
                    {
                        drawMesh(thread, command_buffer, *mesh, uniform_buffer, count);
                    }
                }
            }
        }
    }

    void LandscapeEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::Buffer uniform_buffer, vk::DeviceSize count)
    {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = uniform_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        vk::DescriptorImageInfo image_info;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        //TODO: debug texture? probably AYAYA
        if (mesh.texture)
        {
            image_info.imageView = *mesh.texture->image_view;
            image_info.sampler = *mesh.texture->sampler;
        }

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &buffer_info;

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &image_info;

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        vk::DeviceSize offsets = 0;
        command_buffer.bindVertexBuffers(0, *mesh.vertex_buffer->buffer, offsets, thread->engine->renderer.dispatch);
        command_buffer.bindIndexBuffer(*mesh.index_buffer->buffer, offsets, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        command_buffer.drawIndexed(mesh.getIndexCount(), count, 0, 0, 0, thread->engine->renderer.dispatch);
    }

    void LandscapeEntityInitTask::populateInstanceBuffer(WorkerThread* thread)
    {
        vk::DeviceSize buffer_size = sizeof(LandscapeEntity::InstanceInfo) * instance_info.size();

        staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = thread->engine->renderer.device->mapMemory(staging_buffer->memory, staging_buffer->memory_offset, buffer_size, {}, thread->engine->renderer.dispatch);
        memcpy(data, instance_info.data(), buffer_size);
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

        vk::BufferMemoryBarrier barrier;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = *entity->instance_buffer->buffer;
        barrier.size = VK_WHOLE_SIZE;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, barrier, nullptr, thread->engine->renderer.dispatch);

        vk::BufferCopy copy_region = {};
        copy_region.size = buffer_size;
        command_buffer->copyBuffer(*staging_buffer->buffer, *entity->instance_buffer->buffer, copy_region, thread->engine->renderer.dispatch);

        command_buffer->end(thread->engine->renderer.dispatch);

        thread->primary_buffers[thread->engine->renderer.getCurrentImage()].push_back(*command_buffer);
    }
}
