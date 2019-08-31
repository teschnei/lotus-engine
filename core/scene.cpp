#include "scene.h"
#include "entity/renderable_entity.h"
#include "core.h"
#include "renderer/renderer.h"
#include "task/acceleration_build.h"

namespace lotus
{
    void Scene::render()
    {
        for (const auto& entity : entities)
        {
            if (auto renderable_entity = std::dynamic_pointer_cast<RenderableEntity>(entity))
            {
                renderable_entity->render(engine, renderable_entity);
                if (rebuild_as)
                {
                    renderable_entity->populate_AS(top_level_as.get());
                }
            }
        }
        if (rebuild_as)
        {
            engine->worker_pool.addWork(std::make_unique<AccelerationBuildTask>(engine->renderer.getCurrentImage(), top_level_as));
            rebuild_as = false;
        }
    }

    void Scene::AddEntity(std::shared_ptr<Entity>&& entity)
    {
        entities.push_back(entity);
        if (engine->renderer.RTXEnabled())
        {
            top_level_as = std::make_shared<TopLevelAccelerationStructure>(engine, true);
            rebuild_as = true;
        }
    }
}

