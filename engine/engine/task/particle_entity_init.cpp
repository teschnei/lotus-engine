#include "particle_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/particle.h"

namespace lotus
{
    ParticleEntityInitTask::ParticleEntityInitTask(const std::shared_ptr<Particle>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void ParticleEntityInitTask::Process(WorkerThread* thread)
    {
        entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        createStaticCommandBuffers(thread);
    }

    void ParticleEntityInitTask::createStaticCommandBuffers(WorkerThread* thread)
    {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        //entity->shadowmap_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

        if (thread->engine->renderer.RasterizationEnabled())
        {
            for (size_t i = 0; i < entity->command_buffers.size(); ++i)
            {
                auto& command_buffer = entity->command_buffers[i];
                vk::CommandBufferInheritanceInfo inheritInfo = {};
                inheritInfo.renderPass = *thread->engine->renderer.gbuffer_render_pass;
                inheritInfo.framebuffer = *thread->engine->renderer.gbuffer.frame_buffer;
                inheritInfo.subpass = 1;

                vk::CommandBufferBeginInfo beginInfo = {};
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
                beginInfo.pInheritanceInfo = &inheritInfo;

                command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.graphics_pipeline, thread->engine->renderer.dispatch);

                vk::DescriptorBufferInfo camera_buffer_info;
                camera_buffer_info.buffer = thread->engine->camera->view_proj_ubo->buffer;
                camera_buffer_info.offset = i * sizeof(Camera::CameraData);
                camera_buffer_info.range = sizeof(Camera::CameraData);

                vk::DescriptorBufferInfo model_buffer_info;
                model_buffer_info.buffer = entity->uniform_buffer->buffer;
                model_buffer_info.offset = i * (sizeof(RenderableEntity::UniformBufferObject));
                model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

                vk::DescriptorBufferInfo mesh_info;
                mesh_info.buffer = thread->engine->renderer.mesh_info_buffer->buffer;
                mesh_info.offset = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index * i;
                mesh_info.range = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index;

                std::array<vk::WriteDescriptorSet, 3> descriptorWrites = {};

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &camera_buffer_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 2;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pBufferInfo = &model_buffer_info;

                descriptorWrites[2].dstSet = nullptr;
                descriptorWrites[2].dstBinding = 3;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pBufferInfo = &mesh_info;

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

                drawModel(thread, *command_buffer, false, *thread->engine->renderer.pipeline_layout, i);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.blended_graphics_pipeline, thread->engine->renderer.dispatch);

                drawModel(thread, *command_buffer, true, *thread->engine->renderer.pipeline_layout, i);

                command_buffer->end(thread->engine->renderer.dispatch);
            }
        }

       /* if (thread->engine->renderer.render_mode == RenderMode::Rasterization)
        {
            for (size_t i = 0; i < entity->shadowmap_buffers.size(); ++i)
            {
                auto& command_buffer = entity->shadowmap_buffers[i];
                vk::CommandBufferInheritanceInfo inheritInfo = {};
                inheritInfo.renderPass = *thread->engine->renderer.shadowmap_render_pass;
                //used in multiple framebuffers so we can't give the hint
                //inheritInfo.framebuffer = *thread->engine->renderer.shadowmap_frame_buffer;

                vk::CommandBufferBeginInfo beginInfo = {};
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
                beginInfo.pInheritanceInfo = &inheritInfo;

                command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

                vk::DescriptorBufferInfo buffer_info;
                buffer_info.buffer = entity->uniform_buffer->buffer;
                buffer_info.offset = i * sizeof(RenderableEntity::UniformBufferObject);
                buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

                vk::DescriptorBufferInfo cascade_buffer_info;
                cascade_buffer_info.buffer = thread->engine->camera->cascade_data_ubo->buffer;
                cascade_buffer_info.offset = i * sizeof(thread->engine->camera->cascade_data);
                cascade_buffer_info.range = sizeof(thread->engine->camera->cascade_data);

                std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 2;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &buffer_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 3;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pBufferInfo = &cascade_buffer_info;

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

                command_buffer->setDepthBias(1.25f, 0, 1.75f);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.shadowmap_pipeline, thread->engine->renderer.dispatch);
                drawModel(thread, *command_buffer, false, *thread->engine->renderer.shadowmap_pipeline_layout, i);
                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.blended_shadowmap_pipeline, thread->engine->renderer.dispatch);
                drawModel(thread, *command_buffer, true, *thread->engine->renderer.shadowmap_pipeline_layout, i);

                command_buffer->end(thread->engine->renderer.dispatch);
            }
        }*/
    }

    void ParticleEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, bool transparency, vk::PipelineLayout layout, size_t image)
    {
        for (size_t model_i = 0; model_i < entity->models.size(); ++model_i)
        {
            Model* model = entity->models[model_i].get();
            if (!model->meshes.empty())
            {
                uint32_t material_index = 0;
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        command_buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, {0}, thread->engine->renderer.dispatch);
                        drawMesh(thread, command_buffer, *mesh, layout, material_index);
                    }
                }
            }
        }
    }

    void ParticleEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t material_index)
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

        command_buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0, thread->engine->renderer.dispatch);
    }

    ParticleEntityReInitTask::ParticleEntityReInitTask(const std::shared_ptr<Particle>& entity) : ParticleEntityInitTask(entity)
    {
    }

    void ParticleEntityReInitTask::Process(WorkerThread* thread)
    {
        createStaticCommandBuffers(thread);
    }
}
