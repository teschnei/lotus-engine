module;

#include <coroutine>
#include <cstring>
#include <memory>
#include <vulkan/vulkan.h>

export module lotus:entity.component.particle_raytrace;

import :core.engine;
import :entity.component;
import :entity.component.particle;
import :entity.component.render_base;
import :renderer.memory;
import :renderer.raytrace_query;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class ParticleRaytraceComponent : public Component<ParticleRaytraceComponent, After<ParticleComponent, RenderBaseComponent>>
{
public:
    explicit ParticleRaytraceComponent(Entity*, Engine* engine, ParticleComponent& particle, RenderBaseComponent& base);

    Task<> tick(time_point time, duration delta);

protected:
    const ParticleComponent& particle_component;
    const RenderBaseComponent& base_component;
};

ParticleRaytraceComponent::ParticleRaytraceComponent(Entity* _entity, Engine* _engine, ParticleComponent& _particle_component,
                                                     RenderBaseComponent& _base_component)
    : Component(_entity, _engine), particle_component(_particle_component), base_component(_base_component)
{
}

Task<> ParticleRaytraceComponent::tick(time_point time, duration delta)
{
    auto [model, info] = particle_component.getModel();

    auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
        .commandPool = *engine->renderer->graphics_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });

    auto command_buffer = std::move(command_buffers[0]);

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    uint32_t image = engine->renderer->getCurrentFrame();
    uint32_t prev_image = engine->renderer->getPreviousFrame();

    if (auto tlas = engine->renderer->raytracer->getTLAS(engine->renderer->getCurrentFrame()))
    {
        // transpose because VK_raytracing_KHR expects row-major
        auto matrix = glm::mat3x4{base_component.getModelMatrixT()};
        throw std::runtime_error("fix me");
        vk::AccelerationStructureInstanceKHR instance{.instanceCustomIndex = info->index,
                                                      .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::Particle),
                                                      .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 4,
                                                      .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                                                      .accelerationStructureReference = model->bottom_level_as->handle};
        memcpy(&instance.transform, &matrix, sizeof(matrix));
        model->bottom_level_as->instanceid = tlas->AddInstance(instance);
    }

    command_buffer->end();
    engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
    engine->worker_pool->gpuResource(std::move(command_buffer));
    co_return;
}
} // namespace lotus::Component
