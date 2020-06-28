#include "scene.h"
#include "entity/renderable_entity.h"
#include "entity/deformable_entity.h"
#include "entity/particle.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"
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
        if (engine->config->renderer.RaytraceEnabled())
        {
            top_level_as[image_index] = std::make_shared<TopLevelAccelerationStructure>(engine, true);
            Model::forEachModel([this, image_index](const std::shared_ptr<Model>& model)
            {
                //TODO: review if this is needed
                //if (model->bottom_level_as && model->lifetime != Lifetime::Long)
                //{
                //    top_level_as[image_index]->AddBLASResource(model.get());
                //}
            });
            for (const auto& entity : entities)
            {
                if (auto deformable_entity = dynamic_cast<DeformableEntity*>(entity.get()))
                {
                    top_level_as[image_index]->AddBLASResource(deformable_entity);
                }
                if (auto particle = dynamic_cast<Particle*>(entity.get()))
                {
                    top_level_as[image_index]->AddBLASResource(particle);
                }
            }
        }
        for (auto& entity : entities)
        {
            entity->render_all(engine, entity);
            if (auto renderable_entity = dynamic_cast<RenderableEntity*>(entity.get()))
            {
                if (engine->config->renderer.RaytraceEnabled())
                {
                    renderable_entity->populate_AS(top_level_as[image_index].get(), image_index);
                }
            }
        }
        if (engine->config->renderer.RaytraceEnabled())
        {
           engine->worker_pool.addWork(std::make_unique<AccelerationBuildTask>(top_level_as[image_index]));
        }
    }

    void Scene::tick_all(time_point time, duration delta)
    {
        tick(time, delta);
        for (const auto& entity : entities)
        {
            entity->tick_all(time, delta);
        }
        entities.erase(std::remove_if(entities.begin(), entities.end(), [](auto& entity)
        {
            return entity->should_remove();
        }), entities.end());
        entities.insert(entities.end(), std::move_iterator(new_entities.begin()), std::move_iterator(new_entities.end()));
        new_entities.clear();
    }
}

