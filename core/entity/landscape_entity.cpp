#include "landscape_entity.h"

namespace lotus
{
    void LandscapeEntity::populate_AS(TopLevelAccelerationStructure* as)
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
                    instance.transform = glm::mat3x4{ glm::transpose(instance_info[offset+i].model) };
                    instance.accelerationStructureHandle = model->bottom_level_as->handle;
                    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                    instance.mask = 0xFF;
                    instance.instanceOffset = 0;
                    instance.instanceId = model->bottom_level_as->resource_index;
                    model->acceleration_instanceid = as->AddInstance(instance);
                }
            }
        }
    }

    void LandscapeEntity::update_AS(TopLevelAccelerationStructure* as)
    {
        //landscape can't move so no need to update
    }
}
