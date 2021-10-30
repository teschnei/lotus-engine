#include "landscape_entity.h"
#include "engine/core.h"
#include "engine/scene.h"
#include "engine/renderer/raytrace_query.h"
#include "engine/renderer/vulkan/renderer.h"

#include "engine/entity/component/component_rewrite_test/instanced_raster_component.h"
#include "engine/entity/component/component_rewrite_test/instanced_raytrace_component.h"

namespace lotus
{
    void LandscapeEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
    }

    Task<> LandscapeEntity::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_await renderWork();
        co_return;
    }

    WorkerTask<> LandscapeEntity::renderWork()
    {
        co_return;
    }

    WorkerTask<> LandscapeEntity::InitWork(std::vector<Test::InstancedModelsComponent::InstanceInfo>&& instance_info, Scene* scene)
    {
        auto models_c = co_await scene->component_runners->addComponent<Test::InstancedModelsComponent>(this, models, instance_info, instance_offsets);
        auto models_raster = co_await scene->component_runners->addComponent<Test::InstancedRasterComponent>(this, *models_c);
        auto models_raytrace = scene->component_runners->addComponent<Test::InstancedRaytraceComponent>(this, *models_c);
        co_return;
    }

    WorkerTask<> LandscapeEntity::ReInitWork()
    {
        co_return;
    }
}
