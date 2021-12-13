#include "deformable_raytrace_component.h"
#include "engine/core.h"
#include "engine/renderer/skeleton.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/game.h"
#include "engine/scene.h"

namespace lotus::Component
{
    DeformableRaytraceComponent::DeformableRaytraceComponent(Entity* _entity, Engine* _engine, const DeformedMeshComponent& _mesh_component, const RenderBaseComponent& _base_component) :
         Component(_entity, _engine), mesh_component(_mesh_component), base_component(_base_component)
    {
    }

    WorkerTask<> DeformableRaytraceComponent::init()
    {
        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto models = mesh_component.getModels();

        for (size_t i = 0; i < models.size(); ++i)
        {
            const Model& model = *models[i];
            if (model.weighted)
            {
                acceleration_structures.push_back(initModelWork(*command_buffer, model, mesh_component.getModelTransformGeometry(i)));
            }
        }
        command_buffer->end();
        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));

        co_return;
    }

    WorkerTask<DeformableRaytraceComponent::ModelAccelerationStructures> DeformableRaytraceComponent::initModel(std::shared_ptr<Model> model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const
    {
        ModelAccelerationStructures acceleration;
        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto models = mesh_component.getModels();

        if (model->weighted)
        {
            acceleration = initModelWork(*command_buffer, *model, model_transform);
        }
        command_buffer->end();
        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));

        co_return std::move(acceleration);
    }

    DeformableRaytraceComponent::ModelAccelerationStructures DeformableRaytraceComponent::initModelWork(vk::CommandBuffer command_buffer, const Model& model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const
    {
        ModelAccelerationStructures acceleration_structure;

        std::vector<std::vector<vk::AccelerationStructureGeometryKHR>> raytrace_geometry;
        std::vector<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>> raytrace_offset_info;
        std::vector<std::vector<uint32_t>> max_primitives;

        raytrace_geometry.resize(engine->renderer->getFrameCount());
        raytrace_offset_info.resize(engine->renderer->getFrameCount());
        max_primitives.resize(engine->renderer->getFrameCount());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < engine->renderer->getFrameCount(); ++image)
            {
                raytrace_geometry[image].push_back(vk::AccelerationStructureGeometryKHR{
                    .geometryType = vk::GeometryTypeKHR::eTriangles,
                    .geometry = { .triangles = vk::AccelerationStructureGeometryTrianglesDataKHR {
                        .vertexFormat = vk::Format::eR32G32B32Sfloat,
                        .vertexData = engine->renderer->gpu->device->getBufferAddress({.buffer = model_transform.vertex_buffers[i][image]->buffer}),
                        .vertexStride = mesh->getVertexInputBindingDescription()[0].stride,
                        .maxVertex = mesh->getMaxIndex(),
                        .indexType = vk::IndexType::eUint16,
                        .indexData = engine->renderer->gpu->device->getBufferAddress({.buffer = mesh->index_buffer->buffer})
                    }},
                    .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque
                });

                raytrace_offset_info[image].push_back({
                    .primitiveCount = static_cast<uint32_t>(mesh->getIndexCount() / 3)
                });

                max_primitives[image].emplace_back(static_cast<uint32_t>(mesh->getIndexCount() / 3));
            }
        }

        for (size_t i = 0; i < engine->renderer->getFrameCount(); ++i)
        {
            if (std::ranges::any_of(raytrace_geometry, [](auto geo) { return !geo.empty(); }))
            {
                acceleration_structure.blas.push_back(std::make_unique<BottomLevelAccelerationStructure>(engine->renderer.get(), command_buffer, std::move(raytrace_geometry[i]),
                    std::move(raytrace_offset_info[i]), std::move(max_primitives[i]), true, model.lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
            }
            else
            {
                acceleration_structure.blas.push_back({});
            }
        }
        return acceleration_structure;
    }

    Task<> DeformableRaytraceComponent::tick(time_point time, duration delta)
    {
        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });

        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto models = mesh_component.getModels();
        uint32_t image = engine->renderer->getCurrentFrame();
        uint32_t prev_image = engine->renderer->getPreviousFrame();

        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            const auto& model_transform = mesh_component.getModelTransformGeometry(i);
            auto& as = acceleration_structures[i].blas[engine->renderer->getCurrentFrame()];

            if (as)
            {
                as->Update(*command_buffer);
                std::vector<vk::DescriptorBufferInfo> vertex_info;
                std::vector<vk::DescriptorBufferInfo> vertex_prev_info;
                std::vector<vk::DescriptorBufferInfo> index_info;
                std::vector<vk::DescriptorBufferInfo> material_info;
                std::vector<vk::DescriptorImageInfo> texture_info;

                for (size_t j = 0; j < model->meshes.size(); ++j)
                {
                    const auto& mesh = model->meshes[j];

                    vertex_info.push_back({ model_transform.vertex_buffers[j][image]->buffer, 0, VK_WHOLE_SIZE });
                    vertex_prev_info.push_back({ model_transform.vertex_buffers[j][prev_image]->buffer, 0, VK_WHOLE_SIZE });
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
                    auto& mesh_info = engine->renderer->resources->getMeshInfo(model_transform.resource_index + j);
                    mesh_info.vertex_offset = static_cast<uint32_t>(vertex_index + j);
                    mesh_info.index_offset = static_cast<uint32_t>(index_index + j);
                    mesh_info.material_index = (uint32_t)mesh->material->index;
                    mesh_info.scale = base_component.getScale();
                    mesh_info.billboard = 0;
                    mesh_info.colour = glm::vec4{1.f};
                    mesh_info.uv_offset = glm::vec2{0.f};
                    mesh_info.animation_frame = models[i]->animation_frame;
                    mesh_info.vertex_prev_offset = static_cast<uint32_t>(vertex_prev_index + j);
                    mesh_info.model_prev = base_component.getPrevModelMatrix();
                }

                if (auto tlas = engine->renderer->raytracer->getTLAS(engine->renderer->getCurrentFrame()))
                {
                    //transpose because VK_raytracing_KHR expects row-major
                    auto matrix = glm::mat3x4{ base_component.getModelMatrixT() };
                    vk::AccelerationStructureInstanceKHR instance
                    {
                        .instanceCustomIndex = model_transform.resource_index,
                        .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::DynamicEntities),
                        .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 0,
                        .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                        .accelerationStructureReference = as->handle
                    };
                    memcpy(&instance.transform, &matrix, sizeof(matrix));
                    as->instanceid = tlas->AddInstance(instance);
                }
            }
        }

        command_buffer->end();
        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return;
    }

    void DeformableRaytraceComponent::replaceModelIndex(ModelAccelerationStructures&& acceleration, uint32_t index)
    {
        std::swap(acceleration_structures[index], acceleration);

        engine->worker_pool->gpuResource(std::move(acceleration));
    }
}
