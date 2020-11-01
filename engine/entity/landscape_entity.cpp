#include "landscape_entity.h"
#include "engine/core.h"
#include "engine/renderer/raytrace_query.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/entity_initializers/landscape_entity.h"

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
                    //glm is column-major so we have to transpose the model matrix for RTX
                    auto matrix = glm::mat3x4{ instance_info[offset+i].model_t };
                    engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, model->bottom_level_as->resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::LevelGeometry), 2);
                }
            }
        }
    }

    void LandscapeEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        //landscape can't move so no need to update
    }

    WorkerTask<> LandscapeEntity::InitWork(std::vector<InstanceInfo>&& instance_info)
    {
        //priority: 0
        auto initializer = std::make_unique<LandscapeEntityInitializer>(this, std::move(instance_info));
        engine->renderer->initEntity(initializer.get(), engine);
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    WorkerTask<> LandscapeEntity::ReInitWork()
    {
        //priority: 0
        auto initializer = std::make_unique<LandscapeEntityInitializer>(this, std::vector<InstanceInfo>());
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }
}
