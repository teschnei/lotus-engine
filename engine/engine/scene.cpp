#include "scene.h"
#include "entity/renderable_entity.h"
#include "entity/deformable_entity.h"
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
        if (engine->renderer.RTXEnabled())
        {
            top_level_as[image_index] = std::make_shared<TopLevelAccelerationStructure>(engine, true);
            Model::forEachModel([this, image_index](const std::shared_ptr<Model>& model)
            {
                if (model->bottom_level_as && model->lifetime != Lifetime::Long)
                {
                    top_level_as[image_index]->AddBLASResource(model.get());
                }
            });
            for (const auto& entity : entities)
            {
                if (auto deformable_entity = dynamic_cast<DeformableEntity*>(entity.get()))
                {
                    top_level_as[image_index]->AddBLASResource(deformable_entity);
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
                    renderable_entity->populate_AS(top_level_as[image_index].get(), image_index);
                }
            }
        }
        if (engine->renderer.RTXEnabled())
        {
           engine->worker_pool.addWork(std::make_unique<AccelerationBuildTask>(top_level_as[image_index]));
        }
    }
}

