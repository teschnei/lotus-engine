#include "instanced_raytrace_component.h"
#include "engine/core.h"
#include "engine/game.h"

namespace lotus::Test
{
    InstancedRaytraceComponent::InstancedRaytraceComponent(Entity* _entity, Engine* _engine, InstancedModelsComponent& models) :
         Component(_entity, _engine, models)
    {
    }

    Task<> InstancedRaytraceComponent::tick(time_point time, duration delta)
    {
        auto& [models_component] = dependencies;
        auto models = models_component.getModels();

        uint32_t image = engine->renderer->getCurrentImage();

        if (auto tlas = engine->renderer->raytracer->getTLAS(image))
        {
            for (size_t i = 0; i < models.size(); ++i)
            {
                const auto& model = models[i];
                auto& as = model->bottom_level_as;
                auto [offset, count] = models_component.getInstanceOffset(model->name);

                if (count > 0 && !model->meshes.empty() && as)
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        //transpose because VK_raytracing_KHR expects row-major
                        auto matrix = glm::mat3x4{ models_component.getInstanceInfo(offset + i).model_t };
                        vk::AccelerationStructureInstanceKHR instance
                        {
                            .instanceCustomIndex = model->resource_index,
                            .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::LevelGeometry),
                            .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 2,
                            .flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                            .accelerationStructureReference = as->handle
                        };
                        memcpy(&instance.transform, &matrix, sizeof(matrix));
                        // ???
                        as->instanceid = tlas->AddInstance(instance);
                    }
                }
            }
        }
        co_return;
    }
}
