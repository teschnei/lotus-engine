#include "scene.h"
#include "entity/renderable_entity.h"
#include "core.h"
#include "renderer/renderer.h"
#include "task/acceleration_build.h"

namespace lotus
{
    Scene::Scene(Engine* _engine) : engine(_engine)
    {
        top_level_as.resize(engine->renderer.getImageCount());
    }

    void Scene::render()
    {
        uint32_t image_index = engine->renderer.getCurrentImage();
        if (engine->renderer.RTXEnabled() && rebuild_as)
        {
            top_level_as[image_index] = std::make_shared<TopLevelAccelerationStructure>(engine, true);
            Model::forEachModel([this, image_index](const std::shared_ptr<Model>& model)
            {
                if (model->bottom_level_as)
                {
                    top_level_as[image_index]->AddBLASResource(model.get());
                }
            });
            for (const auto& entity : entities)
            {
                if (auto renderable_entity = std::dynamic_pointer_cast<RenderableEntity>(entity))
                {
                    if (renderable_entity->animation_component)
                    {
                        top_level_as[image_index]->AddBLASResource(renderable_entity.get());
                    }
                }
            }
        }
        for (auto& entity : entities)
        {
            entity->render_all(engine, entity);
            if (auto renderable_entity = dynamic_cast<RenderableEntity*>(entity.get()))
            {
                if (engine->renderer.RTXEnabled())
                {
                    if (rebuild_as)
                    {
                        renderable_entity->populate_AS(top_level_as[image_index].get(), image_index);
                    }
                    else
                    {
                        renderable_entity->update_AS(top_level_as[image_index].get(), image_index);
                    }
                }
            }
        }
        if (engine->renderer.RTXEnabled())
        {
            engine->worker_pool.addWork(std::make_unique<AccelerationBuildTask>(engine->renderer.getCurrentImage(), top_level_as[image_index]));
            //rebuild_as = false;
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

