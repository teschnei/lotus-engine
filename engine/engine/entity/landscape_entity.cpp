#include "landscape_entity.h"
#include "engine/renderer/raytrace_query.h"
#include "engine/task/landscape_entity_init.h"
#include "engine/renderer/vulkan/renderer.h"

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
                    vk::AccelerationStructureInstanceKHR instance{};
                    //glm is column-major so we have to transpose the model matrix for RTX
                    auto matrix = glm::mat3x4{ instance_info[offset+i].model_t };
                    memcpy(&instance.transform, &matrix, sizeof(matrix));
                    instance.accelerationStructureReference = model->bottom_level_as->handle;
                    instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
                    instance.mask = static_cast<uint32_t>(Raytracer::ObjectFlags::LevelGeometry);
                    instance.instanceShaderBindingTableRecordOffset = Renderer::shaders_per_group * 2;
                    instance.instanceCustomIndex = model->bottom_level_as->resource_index;
                    model->bottom_level_as->instanceid = as->AddInstance(instance);
                }
            }
        }
    }

    void LandscapeEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        //landscape can't move so no need to update
    }

    std::unique_ptr<WorkItem> LandscapeEntity::recreate_command_buffers(std::shared_ptr<Entity>& sp)
    {
        return std::make_unique<LandscapeEntityReInitTask>(std::static_pointer_cast<LandscapeEntity>(sp));
    }
}
