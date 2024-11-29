module;

#include <coroutine>
#include <cstring>
#include <memory>
#include <span>
#include <vulkan/vulkan.h>

export module lotus:entity.component.instanced_raytrace;

import :core.engine;
import :entity.component;
import :entity.component.instanced_models;
import :renderer.acceleration_structure;
import :renderer.memory;
import :renderer.raytrace_query;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class InstancedRaytraceComponent : public Component<InstancedRaytraceComponent, After<InstancedModelsComponent>>
{
public:
    explicit InstancedRaytraceComponent(Entity*, Engine* engine, const InstancedModelsComponent& models);

    Task<> tick(time_point time, duration delta);

protected:
    const InstancedModelsComponent& models_component;
};

InstancedRaytraceComponent::InstancedRaytraceComponent(Entity* _entity, Engine* _engine, const InstancedModelsComponent& _models_component)
    : Component(_entity, _engine), models_component(_models_component)
{
}

Task<> InstancedRaytraceComponent::tick(time_point time, duration delta)
{
    auto models = models_component.getModels();

    uint32_t image = engine->renderer->getCurrentFrame();

    if (auto tlas = engine->renderer->raytracer->getTLAS(image))
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i].model;
            auto mesh_offset = models[i].mesh_infos->index;
            auto& as = model->bottom_level_as;
            auto [offset, count] = models_component.getInstanceOffset(model->name);

            if (count > 0 && !model->meshes.empty() && as)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    // transpose because VK_raytracing_KHR expects row-major
                    auto matrix = glm::mat3x4{models_component.getInstanceInfo(offset + i).model_t};
                    vk::AccelerationStructureInstanceKHR instance{.instanceCustomIndex = mesh_offset,
                                                                  .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::LevelGeometry),
                                                                  .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 2,
                                                                  .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                                                                  .accelerationStructureReference = as->handle};
                    memcpy(&instance.transform, &matrix, sizeof(matrix));
                    // ???
                    as->instanceid = tlas->AddInstance(instance);
                }
            }
        }
    }
    co_return;
}
} // namespace lotus::Component
