#include "particle_raytrace_component.h"
#include "engine/core.h"
#include "engine/renderer/skeleton.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    ParticleRaytraceComponent::ParticleRaytraceComponent(Entity* _entity, Engine* _engine, ParticleComponent& _particle_component, RenderBaseComponent& _base_component) :
         Component(_entity, _engine), particle_component(_particle_component), base_component(_base_component)
    {
    }

    Task<> ParticleRaytraceComponent::tick(time_point time, duration delta)
    {
        auto models = particle_component.getModels();

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });

        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        uint32_t image = engine->renderer->getCurrentFrame();
        uint32_t prev_image = engine->renderer->getPreviousFrame();

        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];

            std::vector<vk::DescriptorBufferInfo> vertex_info;
            std::vector<vk::DescriptorBufferInfo> vertex_prev_info;
            std::vector<vk::DescriptorBufferInfo> index_info;
            std::vector<vk::DescriptorBufferInfo> material_info;
            std::vector<vk::DescriptorImageInfo> texture_info;

            for (size_t j = 0; j < model->meshes.size(); ++j)
            {
                const auto& mesh = model->meshes[j];
                auto quad_size = mesh->getVertexStride() * 6;
                auto vertex_buffer_offset = particle_component.current_sprite * quad_size;
                vertex_info.push_back({ mesh->vertex_buffer->buffer, vertex_buffer_offset, quad_size });
                //vertex_prev_info.push_back({ mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE });
                index_info.push_back({ mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE });
                texture_info.push_back({ *mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal });
                auto [buffer, offset] = mesh->material->getBuffer();
                material_info.push_back({ buffer, offset, Material::getMaterialBufferSize(engine) });
            }

            uint16_t vertex_index = engine->renderer->resources->pushVertexInfo(vertex_info);
            uint16_t vertex_prev_index = engine->renderer->resources->pushVertexPrevInfo(vertex_prev_info);
            uint16_t index_index = engine->renderer->resources->pushIndexInfo(index_info);
            uint16_t material_texture_index = engine->renderer->resources->pushMaterialTextureInfo(material_info, texture_info);

            for (size_t j = 0; j < model->meshes.size(); ++j)
            {
                const auto& mesh = model->meshes[j];
                mesh->material->index = material_texture_index + j;
                auto& mesh_info = engine->renderer->resources->getMeshInfo(particle_component.resource_index);
                mesh_info.vertex_offset = static_cast<uint32_t>(vertex_index + j);
                mesh_info.index_offset = static_cast<uint32_t>(index_index + j);
                mesh_info.indices = (uint32_t)mesh->getIndexCount();
                mesh_info.material_index = (uint32_t)mesh->material->index;
                mesh_info.scale = base_component.getScale();
                mesh_info.billboard = base_component.getBillboard();
                mesh_info.vertex_prev_offset = static_cast<uint32_t>(vertex_prev_index + j);
                mesh_info.model_prev = base_component.getPrevModelMatrix();
            }

            if (auto tlas = engine->renderer->raytracer->getTLAS(engine->renderer->getCurrentFrame()))
            {
                //transpose because VK_raytracing_KHR expects row-major
                auto matrix = glm::mat3x4{ base_component.getModelMatrixT() };
                vk::AccelerationStructureInstanceKHR instance
                {
                    .instanceCustomIndex = static_cast<uint32_t>(particle_component.resource_index),
                    .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::Particle),
                    .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 4,
                    .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                    .accelerationStructureReference = model->bottom_level_as->handle
                };
                memcpy(&instance.transform, &matrix, sizeof(matrix));
                model->bottom_level_as->instanceid = tlas->AddInstance(instance);
            }
        }

        command_buffer->end();
        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return;
    }
}
