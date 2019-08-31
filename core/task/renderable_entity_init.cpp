#include "renderable_entity_init.h"
#include "worker_thread.h"
#include "core.h"
#include "renderer/renderer.h"

namespace lotus
{
    RenderableEntityInitTask::RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void RenderableEntityInitTask::Process(WorkerThread* thread)
    {
        createBuffers(thread, entity.get());

        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->command_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

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

            for (const auto& model : entity->models)
            {
                drawModel(thread, *command_buffer, *model, i * sizeof(RenderableEntity::UniformBufferObject));
            }

            //TODO: transparent meshes
            //command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_graphics_pipeline, thread->engine->renderer.dispatch);

            //for (const auto& model : entity->models)
            //{
            //    drawModel(thread, *command_buffer, *model, *entity->uniform_buffers[i]->buffer, true);
            //}

            command_buffer->end(thread->engine->renderer.dispatch);
        }
    }

    void RenderableEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, const Model& model, vk::DeviceSize uniform_buffer_offset)
    {
        for (const auto& mesh : model.meshes)
        { 
            if (mesh->vertex_buffer && !mesh->has_transparency)
            {
                drawMesh(thread, command_buffer, *mesh, uniform_buffer_offset);
            }
        }
    }

    void RenderableEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::DeviceSize uniform_buffer_offset)
    {
        vk::DeviceSize offsets = 0;
        command_buffer.bindVertexBuffers(0, mesh.vertex_buffer->buffer, offsets, thread->engine->renderer.dispatch);
        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, offsets, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = entity->uniform_buffer->buffer;
        buffer_info.offset = uniform_buffer_offset;
        buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        vk::DescriptorImageInfo image_info;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        image_info.imageView = *mesh.texture->image_view;
        image_info.sampler = *mesh.texture->sampler;

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

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0, thread->engine->renderer.dispatch);
    }

    void RenderableEntityInitTask::createBuffers(WorkerThread* thread, RenderableEntity* _entity)
    {
        _entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}
