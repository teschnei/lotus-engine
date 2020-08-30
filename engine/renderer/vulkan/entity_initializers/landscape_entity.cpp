#include "landscape_entity.h"

#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/task/landscape_entity_init.h"
#include "engine/entity/landscape_entity.h"
#include "engine/renderer/vulkan/raytrace/renderer_raytrace.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"
#include "engine/renderer/vulkan/hybrid/renderer_hybrid.h"

namespace lotus
{
    LandscapeEntityInitializer::LandscapeEntityInitializer(Entity* _entity, std::vector<LandscapeEntity::InstanceInfo>&& _instance_info, LandscapeEntityInitTask* _task) :
        EntityInitializer(_entity), instance_info(std::move(_instance_info)), task(_task)
    {
    }

    void LandscapeEntityInitializer::initEntity(RendererRaytrace* renderer, WorkerThread* thread)
    {
        createBuffers(renderer, thread);
    }

    void LandscapeEntityInitializer::drawEntity(RendererRaytrace* renderer, WorkerThread* thread)
    {
    }

    void LandscapeEntityInitializer::initEntity(RendererRasterization* renderer, WorkerThread* thread)
    {
        createBuffers(renderer, thread);
    }

    void LandscapeEntityInitializer::drawEntity(RendererRasterization* renderer, WorkerThread* thread)
    {
        auto entity = static_cast<LandscapeEntity*>(this->entity);
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(renderer->getImageCount());
        
        entity->command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        entity->shadowmap_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        for (int i = 0; i < entity->command_buffers.size(); ++i)
        {
            auto& command_buffer = entity->command_buffers[i];
            vk::CommandBufferInheritanceInfo inheritInfo = {};
            inheritInfo.renderPass = *renderer->gbuffer_render_pass;
            inheritInfo.framebuffer = *renderer->gbuffer.frame_buffer;

            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            beginInfo.pInheritanceInfo = &inheritInfo;

            command_buffer->begin(beginInfo);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.graphics_pipeline);

            vk::DescriptorBufferInfo buffer_info;
            buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
            buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
            buffer_info.range = sizeof(Camera::CameraData);

            //vk::DescriptorBufferInfo mesh_info;
            //mesh_info.buffer = renderer->mesh_info_buffer->buffer;
            //mesh_info.offset = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index * i;
            //mesh_info.range = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index;

            std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

            descriptorWrites[0].dstSet = nullptr;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &buffer_info;

            //descriptorWrites[1].dstSet = nullptr;
            //descriptorWrites[1].dstBinding = 3;
            //descriptorWrites[1].dstArrayElement = 0;
            //descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
            //descriptorWrites[1].descriptorCount = 1;
            //descriptorWrites[1].pBufferInfo = &mesh_info;

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

            drawModel(thread, *command_buffer, false, *renderer->pipeline_layout);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.blended_graphics_pipeline);

            drawModel(thread, *command_buffer, true, *renderer->pipeline_layout);

            command_buffer->end();
        }

        for (size_t i = 0; i < entity->shadowmap_buffers.size(); ++i)
        {
            auto& command_buffer = entity->shadowmap_buffers[i];
            vk::CommandBufferInheritanceInfo inheritInfo = {};
            inheritInfo.renderPass = *renderer->shadowmap_render_pass;
            //used in multiple framebuffers so we can't give the hint
            //inheritInfo.framebuffer = *renderer->shadowmap_frame_buffer;

            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            beginInfo.pInheritanceInfo = &inheritInfo;

            command_buffer->begin(beginInfo);

            vk::DescriptorBufferInfo buffer_info;
            buffer_info.buffer = entity->uniform_buffer->buffer;
            buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
            buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

            vk::DescriptorBufferInfo cascade_buffer_info;
            cascade_buffer_info.buffer = renderer->camera_buffers.cascade_data_ubo->buffer;
            cascade_buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(renderer->cascade_data));
            cascade_buffer_info.range = sizeof(renderer->cascade_data);

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

            descriptorWrites[0].dstSet = nullptr;
            descriptorWrites[0].dstBinding = 0;
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

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->shadowmap_pipeline_layout, 0, descriptorWrites);

            command_buffer->setDepthBias(1.25f, 0, 1.75f);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.shadowmap_pipeline);
            drawModel(thread, *command_buffer, false, *renderer->shadowmap_pipeline_layout);
            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.blended_shadowmap_pipeline);
            drawModel(thread, *command_buffer, true, *renderer->shadowmap_pipeline_layout);

            command_buffer->end();
        }
    }

    void LandscapeEntityInitializer::initEntity(RendererHybrid* renderer, WorkerThread* thread)
    {
        createBuffers(renderer, thread);
    }

    void LandscapeEntityInitializer::drawEntity(RendererHybrid* renderer, WorkerThread* thread)
    {
        auto entity = static_cast<LandscapeEntity*>(this->entity);
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(renderer->getImageCount());
        
        entity->command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        for (int i = 0; i < entity->command_buffers.size(); ++i)
        {
            auto& command_buffer = entity->command_buffers[i];
            vk::CommandBufferInheritanceInfo inheritInfo = {};
            inheritInfo.renderPass = *renderer->gbuffer_render_pass;
            inheritInfo.framebuffer = *renderer->gbuffer.frame_buffer;

            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            beginInfo.pInheritanceInfo = &inheritInfo;

            command_buffer->begin(beginInfo);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.graphics_pipeline);

            vk::DescriptorBufferInfo buffer_info;
            buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
            buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
            buffer_info.range = sizeof(Camera::CameraData);

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = renderer->mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(RendererHybrid::MeshInfo) * RendererHybrid::max_acceleration_binding_index * i;
            mesh_info.range = sizeof(RendererHybrid::MeshInfo) * RendererHybrid::max_acceleration_binding_index;

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

            descriptorWrites[0].dstSet = nullptr;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &buffer_info;

            descriptorWrites[1].dstSet = nullptr;
            descriptorWrites[1].dstBinding = 3;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &mesh_info;

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

            drawModel(thread, *command_buffer, false, *renderer->pipeline_layout);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *renderer->landscape_pipeline_group.blended_graphics_pipeline);

            drawModel(thread, *command_buffer, true, *renderer->pipeline_layout);

            command_buffer->end();
        }
    }

    void LandscapeEntityInitializer::createBuffers(Renderer* renderer, WorkerThread* thread)
    {
        auto entity = static_cast<LandscapeEntity*>(this->entity);
        entity->uniform_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->mesh_index_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(uint32_t)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), {}));
        entity->mesh_index_buffer_mapped = static_cast<uint8_t*>(entity->mesh_index_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(uint32_t)) * renderer->getImageCount(), {}));

        vk::DeviceSize buffer_size = sizeof(LandscapeEntity::InstanceInfo) * instance_info.size();

        staging_buffer = renderer->gpu->memory_manager->GetBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = staging_buffer->map(0, buffer_size, {});
        memcpy(data, instance_info.data(), buffer_size);
        staging_buffer->unmap();

        entity->instance_info = std::move(instance_info);

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::BufferMemoryBarrier barrier;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = entity->instance_buffer->buffer;
        barrier.size = VK_WHOLE_SIZE;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, barrier, nullptr);

        vk::BufferCopy copy_region = {};
        copy_region.size = buffer_size;
        command_buffer->copyBuffer(staging_buffer->buffer, entity->instance_buffer->buffer, copy_region);

        command_buffer->end();

        task->graphics.primary = *command_buffer;
    }

    void LandscapeEntityInitializer::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, bool transparency, vk::PipelineLayout layout)
    {
        auto entity = static_cast<LandscapeEntity*>(this->entity);
        for (const auto& model : entity->models)
        {
            auto [offset, count] = entity->instance_offsets[model->name];
            if (count > 0 && !model->meshes.empty())
            {
                command_buffer.bindVertexBuffers(1, entity->instance_buffer->buffer, offset * sizeof(LandscapeEntity::InstanceInfo));
                uint32_t material_index = 1;
                for (size_t i = 0; i < model->meshes.size(); ++i)
                {
                    auto& mesh = model->meshes[i];
                    if (mesh->has_transparency == transparency)
                    {
                        if (model->bottom_level_as)
                        {
                            material_index = model->bottom_level_as->resource_index + i;
                        }
                        drawMesh(thread, command_buffer, *mesh, count, layout, material_index);
                    }
                }
            }
        }
    }

    void LandscapeEntityInitializer::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, uint32_t count, vk::PipelineLayout layout, uint32_t material_index)
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

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites);

        command_buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindVertexBuffers(0, mesh.vertex_buffer->buffer, {0});
        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, {0}, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), count, 0, 0, 0);
    }
}
