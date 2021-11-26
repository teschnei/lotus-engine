#include "static_collision_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    StaticCollisionComponent::StaticCollisionComponent(Entity* _entity, Engine* _engine, std::vector<std::shared_ptr<Model>> _models) :
        Component(_entity, _engine), models(_models)
    {
    }

    Task<> StaticCollisionComponent::tick(time_point time, duration delta)
    {
        uint32_t image = engine->renderer->getCurrentFrame();

        if (engine->renderer->raytracer)
        {
            if (auto tlas = engine->renderer->raytracer->getTLAS(image))
            {
                for (size_t i = 0; i < models.size(); ++i)
                {
                    const auto& model = models[i];
                    auto& as = model->bottom_level_as;

                    if (!model->meshes.empty() && as)
                    {
                        //transpose because VK_raytracing_KHR expects row-major
                        auto matrix = glm::mat3x4{ 1.f };
                        vk::AccelerationStructureInstanceKHR instance
                        {
                            .instanceCustomIndex = 0,
                            .mask = static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::LevelCollision),
                            .instanceShaderBindingTableRecordOffset = RaytracePipeline::shaders_per_group * 0,
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
