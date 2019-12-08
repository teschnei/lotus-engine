#include "landscape_entity.h"
#include "engine/renderer/raytrace_query.h"

namespace lotus
{
    void LandscapeEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (const auto& model : models)
        {
            auto [offset, count] = instance_offsets[model->name];
            if (count > 0 && !model->meshes.empty() && model->bottom_level_as)
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    VkGeometryInstance instance{};
                    //glm is column-major so we have to transpose the model matrix for RTX
                    instance.transform = glm::mat3x4{ instance_info[offset+i].model_t };
                    instance.accelerationStructureHandle = model->bottom_level_as->handle;
                    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                    instance.mask = static_cast<uint32_t>(Raytracer::ObjectFlags::LevelGeometry);
                    instance.instanceOffset = 32;
                    instance.instanceId = model->bottom_level_as->resource_index;
                    model->bottom_level_as->instanceid = as->AddInstance(instance);
                }
            }
        }
    }

    void LandscapeEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        //landscape can't move so no need to update
    }
}
