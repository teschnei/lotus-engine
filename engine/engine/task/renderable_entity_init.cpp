#include "renderable_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/renderer.h"

namespace lotus
{
    RenderableEntityInitTask::RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void RenderableEntityInitTask::Process(WorkerThread* thread)
    {
        entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        //TODO: need new pipelines for these (also means FFXI::MMB::Vertex bindings don't need to be used

        //vk::CommandBufferAllocateInfo alloc_info;
        //alloc_info.level = vk::CommandBufferLevel::eSecondary;
        //alloc_info.commandPool = *thread->command_pool;
        //alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        //entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        //entity->shadowmap_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

        //if (thread->engine->renderer.RasterizationEnabled())
        //{
        //    for (int i = 0; i < entity->command_buffers.size(); ++i)
        //    {
        //        auto& command_buffer = entity->command_buffers[i];
        //        vk::CommandBufferInheritanceInfo inheritInfo = {};
        //        inheritInfo.renderPass = *thread->engine->renderer.gbuffer_render_pass;
        //        inheritInfo.framebuffer = *thread->engine->renderer.gbuffer.frame_buffer;

        //        vk::CommandBufferBeginInfo beginInfo = {};
        //        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        //        beginInfo.pInheritanceInfo = &inheritInfo;

        //        command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_graphics_pipeline, thread->engine->renderer.dispatch);

        //        vk::DescriptorBufferInfo buffer_info;
        //        buffer_info.buffer = thread->engine->camera.view_proj_ubo->buffer;
        //        buffer_info.offset = i * (sizeof(glm::mat4) * 4);
        //        buffer_info.range = sizeof(glm::mat4) * 4;

        //        std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

        //        descriptorWrites[0].dstSet = nullptr;
        //        descriptorWrites[0].dstBinding = 0;
        //        descriptorWrites[0].dstArrayElement = 0;
        //        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[0].descriptorCount = 1;
        //        descriptorWrites[0].pBufferInfo = &buffer_info;

        //        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        //        drawModel(thread, *command_buffer, false, *thread->engine->renderer.pipeline_layout);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_graphics_pipeline, thread->engine->renderer.dispatch);

        //        drawModel(thread, *command_buffer, true, *thread->engine->renderer.pipeline_layout);

        //        command_buffer->end(thread->engine->renderer.dispatch);
        //    }
        //}

        //if (thread->engine->renderer.render_mode == RenderMode::Rasterization)
        //{
        //    for (size_t i = 0; i < entity->shadowmap_buffers.size(); ++i)
        //    {
        //        auto& command_buffer = entity->shadowmap_buffers[i];
        //        vk::CommandBufferInheritanceInfo inheritInfo = {};
        //        inheritInfo.renderPass = *thread->engine->renderer.shadowmap_render_pass;
        //        //used in multiple framebuffers so we can't give the hint
        //        //inheritInfo.framebuffer = *thread->engine->renderer.shadowmap_frame_buffer;

        //        vk::CommandBufferBeginInfo beginInfo = {};
        //        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        //        beginInfo.pInheritanceInfo = &inheritInfo;

        //        command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline, thread->engine->renderer.dispatch);

        //        vk::DescriptorBufferInfo buffer_info;
        //        buffer_info.buffer = entity->uniform_buffer->buffer;
        //        buffer_info.offset = i * sizeof(RenderableEntity::UniformBufferObject);
        //        buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        //        vk::DescriptorBufferInfo cascade_buffer_info;
        //        cascade_buffer_info.buffer = thread->engine->camera.cascade_data_ubo->buffer;
        //        cascade_buffer_info.offset = i * sizeof(thread->engine->camera.cascade_data);
        //        cascade_buffer_info.range = sizeof(thread->engine->camera.cascade_data);

        //        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

        //        descriptorWrites[0].dstSet = nullptr;
        //        descriptorWrites[0].dstBinding = 0;
        //        descriptorWrites[0].dstArrayElement = 0;
        //        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[0].descriptorCount = 1;
        //        descriptorWrites[0].pBufferInfo = &buffer_info;

        //        descriptorWrites[1].dstSet = nullptr;
        //        descriptorWrites[1].dstBinding = 2;
        //        descriptorWrites[1].dstArrayElement = 0;
        //        descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[1].descriptorCount = 1;
        //        descriptorWrites[1].pBufferInfo = &cascade_buffer_info;

        //        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        //        command_buffer->setDepthBias(1.25f, 0, 1.75f);

        //        drawModel(thread, *command_buffer, false, *thread->engine->renderer.shadowmap_pipeline_layout);
        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_shadowmap_pipeline, thread->engine->renderer.dispatch);
        //        drawModel(thread, *command_buffer, true, *thread->engine->renderer.shadowmap_pipeline_layout);

        //        command_buffer->end(thread->engine->renderer.dispatch);
        //    }
        //}
    }

    void RenderableEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, bool transparency, vk::PipelineLayout layout)
    {
        for (const auto& model : entity->models)
        {
            if (!model->meshes.empty())
            {
                for (const auto& mesh : model->meshes)
                {
                    if (mesh->has_transparency == transparency)
                    {
                        drawMesh(thread, command_buffer, *mesh, layout);
                    }
                }
            }
        }
    }

    void RenderableEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout)
    {
        vk::DescriptorImageInfo image_info;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        //TODO: debug texture? probably AYAYA
        if (mesh.texture)
        {
            image_info.imageView = *mesh.texture->image_view;
            image_info.sampler = *mesh.texture->sampler;
        }

        std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[0].dstBinding = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &image_info;

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        command_buffer.bindVertexBuffers(0, mesh.vertex_buffer->buffer, {0}, thread->engine->renderer.dispatch);
        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, {0}, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0, thread->engine->renderer.dispatch);
    }
}
