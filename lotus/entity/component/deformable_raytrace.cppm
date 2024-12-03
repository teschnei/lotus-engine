module;

#include <algorithm>
#include <coroutine>
#include <cstring>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

export module lotus:entity.component.deformable_raytrace;

import :core.engine;
import :entity.component;
import :entity.component.deformed_mesh;
import :entity.component.render_base;
import :renderer.memory;
import :renderer.raytrace_query;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class DeformableRaytraceComponent : public Component<DeformableRaytraceComponent, After<DeformedMeshComponent, RenderBaseComponent>>
{
public:
    struct ModelAccelerationStructures
    {
        // mesh acceleration structures
        std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> blas;
    };

    explicit DeformableRaytraceComponent(Entity*, Engine* engine, const DeformedMeshComponent& deformed, const RenderBaseComponent& physics);

    WorkerTask<> init();
    Task<> tick(time_point time, duration delta);

    WorkerTask<ModelAccelerationStructures> initModel(const DeformedMeshComponent::ModelInfo& model_info) const;
    void replaceModelIndex(ModelAccelerationStructures&& acceleration, uint32_t index);

protected:
    const DeformedMeshComponent& mesh_component;
    const RenderBaseComponent& base_component;
    std::vector<ModelAccelerationStructures> acceleration_structures;

    ModelAccelerationStructures initModelWork(vk::CommandBuffer command_buffer, const DeformedMeshComponent::ModelInfo& model_info) const;
};

DeformableRaytraceComponent::DeformableRaytraceComponent(Entity* _entity, Engine* _engine, const DeformedMeshComponent& _mesh_component,
                                                         const RenderBaseComponent& _base_component)
    : Component(_entity, _engine), mesh_component(_mesh_component), base_component(_base_component)
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

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto models = mesh_component.getModels();

    for (const auto& info : models)
    {
        if (info.model->weighted)
        {
            acceleration_structures.push_back(initModelWork(*command_buffer, info));
        }
    }
    command_buffer->end();
    engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
    engine->worker_pool->gpuResource(std::move(command_buffer));

    co_return;
}

WorkerTask<DeformableRaytraceComponent::ModelAccelerationStructures> DeformableRaytraceComponent::initModel(const DeformedMeshComponent::ModelInfo& info) const
{
    ModelAccelerationStructures acceleration;
    vk::CommandBufferAllocateInfo alloc_info;

    auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
        .commandPool = *engine->renderer->compute_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    auto command_buffer = std::move(command_buffers[0]);

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto models = mesh_component.getModels();

    if (info.model->weighted)
    {
        acceleration = initModelWork(*command_buffer, info);
    }
    command_buffer->end();
    co_await engine->renderer->async_compute->compute(std::move(command_buffer));

    co_return std::move(acceleration);
}

DeformableRaytraceComponent::ModelAccelerationStructures DeformableRaytraceComponent::initModelWork(vk::CommandBuffer command_buffer,
                                                                                                    const DeformedMeshComponent::ModelInfo& info) const
{
    ModelAccelerationStructures acceleration_structure;

    std::vector<std::vector<vk::AccelerationStructureGeometryKHR>> raytrace_geometry;
    std::vector<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>> raytrace_offset_info;
    std::vector<std::vector<uint32_t>> max_primitives;

    raytrace_geometry.resize(engine->renderer->getFrameCount());
    raytrace_offset_info.resize(engine->renderer->getFrameCount());
    max_primitives.resize(engine->renderer->getFrameCount());
    for (size_t i = 0; i < info.model->meshes.size(); ++i)
    {
        const auto& mesh = info.model->meshes[i];

        for (uint32_t image = 0; image < engine->renderer->getFrameCount(); ++image)
        {
            raytrace_geometry[image].push_back(
                vk::AccelerationStructureGeometryKHR{
                    .geometryType = vk::GeometryTypeKHR::eTriangles,
                    .geometry = {.triangles =
                                     vk::AccelerationStructureGeometryTrianglesDataKHR{
                                         .vertexFormat = vk::Format::eR32G32B32Sfloat,
                                         .vertexData = engine->renderer->gpu->device->getBufferAddress({.buffer = info.vertex_buffers[image]->buffer}) +
                                                       mesh->vertex_offset,
                                         .vertexStride = mesh->getVertexInputBindingDescription()[0].stride,
                                         .maxVertex = mesh->getMaxIndex(),
                                         .indexType = vk::IndexType::eUint16,
                                         .indexData = engine->renderer->gpu->device->getBufferAddress({.buffer = info.model->index_buffer->buffer}) +
                                                      mesh->index_offset}},
                    .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque});

            raytrace_offset_info[image].push_back({.primitiveCount = static_cast<uint32_t>(mesh->getIndexCount() / 3)});

            max_primitives[image].emplace_back(static_cast<uint32_t>(mesh->getIndexCount() / 3));
        }
    }

    for (size_t i = 0; i < engine->renderer->getFrameCount(); ++i)
    {
        if (std::ranges::any_of(raytrace_geometry, [](auto geo) { return !geo.empty(); }))
        {
            acceleration_structure.blas.push_back(std::make_unique<BottomLevelAccelerationStructure>(
                engine->renderer.get(), command_buffer, std::move(raytrace_geometry[i]), std::move(raytrace_offset_info[i]), std::move(max_primitives[i]), true,
                info.model->lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
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

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto models = mesh_component.getModels();
    uint32_t current_frame = engine->renderer->getCurrentFrame();
    uint32_t prev_frame = engine->renderer->getPreviousFrame();

    for (size_t i = 0; i < models.size(); ++i)
    {
        const auto& model = models[i].model;
        auto& as = acceleration_structures[i].blas[current_frame];

        if (as)
        {
            as->Update(*command_buffer);

            if (auto tlas = engine->renderer->raytracer->getTLAS(current_frame))
            {
                // transpose because VK_raytracing_KHR expects row-major
                auto matrix = glm::mat3x4{base_component.getModelMatrixT()};
                vk::AccelerationStructureInstanceKHR instance{.instanceCustomIndex = static_cast<uint32_t>(models[i].mesh_infos[current_frame]->index),
                                                              .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::DynamicEntities),
                                                              .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 0,
                                                              .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                                                              .accelerationStructureReference = as->handle};
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
} // namespace lotus::Component
