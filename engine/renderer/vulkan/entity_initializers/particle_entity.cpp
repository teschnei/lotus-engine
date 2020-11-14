#include "particle_entity.h"

#include "engine/core.h"
#include "engine/entity/particle.h"
#include "engine/renderer/vulkan/raytrace/renderer_raytrace.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"
#include "engine/renderer/vulkan/hybrid/renderer_hybrid.h"

namespace lotus
{
    ParticleEntityInitializer::ParticleEntityInitializer(Entity* _entity) :
        EntityInitializer(_entity)
    {
    }

    void ParticleEntityInitializer::initEntity(RendererRaytrace* renderer, Engine* engine)
    {
        createBuffers(renderer, engine);
    }

    void ParticleEntityInitializer::drawEntity(RendererRaytrace* renderer, Engine* engine)
    {
    }

    void ParticleEntityInitializer::initEntity(RendererRasterization* renderer, Engine* engine)
    {
        createBuffers(renderer, engine);
    }

    void ParticleEntityInitializer::drawEntity(RendererRasterization* renderer, Engine* engine)
    {
        auto entity = static_cast<Particle*>(this->entity);
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *engine->renderer->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(renderer->getImageCount());
        
        entity->command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        
        for (size_t i = 0; i < entity->command_buffers.size(); ++i)
        {
            auto& command_buffer = entity->command_buffers[i];

            vk::CommandBufferInheritanceInfo inherit_info = {};
            inherit_info.renderPass = *renderer->gbuffer_render_pass;
            inherit_info.framebuffer = *renderer->gbuffer.frame_buffer;
            inherit_info.subpass = 1;

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            begin_info.pInheritanceInfo = &inherit_info;

            command_buffer->begin(begin_info);

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
            camera_buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
            camera_buffer_info.range = sizeof(Camera::CameraData);

            vk::DescriptorBufferInfo model_buffer_info;
            model_buffer_info.buffer = entity->uniform_buffer->buffer;
            model_buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
            model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

            //vk::DescriptorBufferInfo mesh_info;
            //mesh_info.buffer = renderer->mesh_info_buffer->buffer;
            //mesh_info.offset = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index * i;
            //mesh_info.range = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index;

            vk::DescriptorBufferInfo material_index_info;
            material_index_info.buffer = entity->mesh_index_buffer->buffer;
            material_index_info.offset = i * renderer->uniform_buffer_align_up(sizeof(uint32_t));
            material_index_info.range = sizeof(uint32_t);

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

            //descriptorWrites[2].dstSet = nullptr;
            //descriptorWrites[2].dstBinding = 3;
            //descriptorWrites[2].dstArrayElement = 0;
            //descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
            //descriptorWrites[2].descriptorCount = 1;
            //descriptorWrites[2].pBufferInfo = &mesh_info;

            descriptorWrites[2].dstSet = nullptr;
            descriptorWrites[2].dstBinding = 4;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &material_index_info;

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

            drawModel(engine, *command_buffer, true, *renderer->pipeline_layout, i);

            command_buffer->end();
        }
    }

    void ParticleEntityInitializer::initEntity(RendererHybrid* renderer, Engine* engine)
    {
        createBuffers(renderer, engine);
    }

    void ParticleEntityInitializer::drawEntity(RendererHybrid* renderer, Engine* engine)
    {
        auto entity = static_cast<Particle*>(this->entity);
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *engine->renderer->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(renderer->getImageCount());
        
        entity->command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        
        for (size_t i = 0; i < entity->command_buffers.size(); ++i)
        {
            auto& command_buffer = entity->command_buffers[i];

            vk::CommandBufferInheritanceInfo inherit_info = {};
            inherit_info.renderPass = *renderer->gbuffer_render_pass;
            inherit_info.framebuffer = *renderer->gbuffer.frame_buffer;
            inherit_info.subpass = 1;

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            begin_info.pInheritanceInfo = &inherit_info;

            command_buffer->begin(begin_info);

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
            camera_buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
            camera_buffer_info.range = sizeof(Camera::CameraData);

            vk::DescriptorBufferInfo model_buffer_info;
            model_buffer_info.buffer = entity->uniform_buffer->buffer;
            model_buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
            model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = renderer->mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(RendererHybrid::MeshInfo) * RendererHybrid::max_acceleration_binding_index * i;
            mesh_info.range = sizeof(RendererHybrid::MeshInfo) * RendererHybrid::max_acceleration_binding_index;

            vk::DescriptorBufferInfo material_index_info;
            material_index_info.buffer = entity->mesh_index_buffer->buffer;
            material_index_info.offset = i * renderer->uniform_buffer_align_up(sizeof(uint32_t));
            material_index_info.range = sizeof(uint32_t);

            std::array<vk::WriteDescriptorSet, 4> descriptorWrites = {};

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

            descriptorWrites[3].dstSet = nullptr;
            descriptorWrites[3].dstBinding = 4;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &material_index_info;

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

            drawModel(engine, *command_buffer, true, *renderer->pipeline_layout, i);

            command_buffer->end();
        }

    }

    void ParticleEntityInitializer::createBuffers(Renderer* renderer, Engine* engine)
    {
        auto entity = static_cast<Particle*>(this->entity);
        entity->uniform_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->mesh_index_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(uint32_t)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), {}));
        entity->mesh_index_buffer_mapped = static_cast<uint8_t*>(entity->mesh_index_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(uint32_t)) * renderer->getImageCount(), {}));
    }

    void ParticleEntityInitializer::drawModel(Engine* engine, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout layout, size_t image)
    {
        auto entity = static_cast<Particle*>(this->entity);
        for (size_t model_i = 0; model_i < entity->models.size(); ++model_i)
        {
            Model* model = entity->models[model_i].get();
            if (!model->meshes.empty())
            {
                //TODO: material_index can only work with one model (or one mesh)
                if (model->meshes.size() > 1)
                    DEBUG_BREAK();
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, { 0 });
                        drawMesh(engine, buffer, *mesh, layout, mesh_i);
                    }
                }
            }
        }
    }

    void ParticleEntityInitializer::drawMesh(Engine* engine, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t mesh_index)
    {
        buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipeline);
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

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites);

        buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, mesh_index);

        buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }
}
