#include "scene.h"
#include "entity/renderable_entity.h"
#include "core.h"
#include "renderer/renderer.h"
#include "task/acceleration_build.h"

namespace lotus
{
    void Scene::render()
    {
        if (engine->renderer.RTXEnabled() && rebuild_as)
        {
            top_level_as = std::make_shared<TopLevelAccelerationStructure>(engine, true);
            Model::forEachModel([this](const std::shared_ptr<Model>& model)
            {
                if (model->bottom_level_as)
                {
                    top_level_as->AddBLASResource(model.get());
                }
            });
        }
        for (const auto& entity : entities)
        {
            if (auto renderable_entity = std::dynamic_pointer_cast<RenderableEntity>(entity))
            {
                renderable_entity->render(engine, renderable_entity);
                if (engine->renderer.RTXEnabled())
                {
                    if (rebuild_as)
                    {
                        renderable_entity->populate_AS(top_level_as.get());
                    }
                    else
                    {
                        renderable_entity->update_AS(top_level_as.get());
                    }
                }
            }
        }
        if (engine->renderer.RTXEnabled())
        {
            engine->worker_pool.addWork(std::make_unique<AccelerationBuildTask>(engine->renderer.getCurrentImage(), top_level_as));
            rebuild_as = false;
        }
    }

    void Scene::RebuildTLAS()
    {
        if (engine->renderer.RTXEnabled())
        {
            rebuild_as = true;
        }
    }
}

