#include "landscape_entity.h"

#include "task/landscape_dat_load.h"
#include "engine/core.h"

void FFXILandscapeEntity::Init(const std::shared_ptr<FFXILandscapeEntity>& sp, const std::string& dat)
{
    engine->worker_pool.addWork(std::make_unique<LandscapeDatLoad>(sp, dat));
}

void FFXILandscapeEntity::populate_AS(lotus::TopLevelAccelerationStructure* as, uint32_t image_index)
{
    auto nodes = quadtree.find(engine->camera->frustum);
    for (const auto& node : nodes)
    {
        auto& [model_offset, instance_info] = model_vec[node];
        auto& model = models[model_offset];
        if (!model->meshes.empty() && model->bottom_level_as)
        {
            lotus::VkGeometryInstance instance{};
            //glm is column-major so we have to transpose the model matrix for RTX
            instance.transform = glm::mat3x4{ instance_info.model_t };
            instance.accelerationStructureHandle = model->bottom_level_as->handle;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            instance.mask = static_cast<uint32_t>(lotus::Raytracer::ObjectFlags::LevelGeometry);
            instance.instanceOffset = 32;
            instance.instanceId = model->bottom_level_as->resource_index;
            model->bottom_level_as->instanceid = as->AddInstance(instance);
        }
    }
}
