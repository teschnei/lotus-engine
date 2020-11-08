#include "scene.h"
#include "entity/renderable_entity.h"
#include "entity/deformable_entity.h"
#include "entity/particle.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"
#include "renderer/vulkan/renderer_raytrace_base.h"

namespace lotus
{
    Scene::Scene(Engine* _engine) : engine(_engine)
    {
        top_level_as.resize(engine->renderer->getImageCount());
    }

    Task<> Scene::render()
    {
        uint32_t image_index = engine->renderer->getCurrentImage();
        std::vector<Task<>> tasks;
        if (engine->config->renderer.RaytraceEnabled())
        {
            top_level_as[image_index] = std::make_shared<TopLevelAccelerationStructure>(static_cast<RendererRaytraceBase*>(engine->renderer.get()), true);
            //TODO: review if this is needed
            /*
            Model::forEachModel([this, image_index](const std::shared_ptr<Model>& model)
            {
                //if (model->bottom_level_as && model->lifetime != Lifetime::Long)
                //{
                //    top_level_as[image_index]->AddBLASResource(model.get());
                //}
            });*/
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
            tasks.push_back(entity->render_all(engine, entity));
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
           co_await top_level_as[image_index]->Build(engine);
        }
        for (auto& task : tasks)
        {
            co_await task;
        }
    }

    Task<> Scene::tick_all(time_point time, duration delta)
    {
        co_await tick(time, delta);
        std::vector<decltype(entities)::value_type::element_type*> entities_p{entities.size()};
        std::ranges::transform(entities, entities_p.begin(), [](auto& c) {return c.get(); });
        for (auto entity : entities_p)
        {
            co_await entity->tick_all(time, delta);
        }
        entities.erase(std::remove_if(entities.begin(), entities.end(), [](auto& entity)
        {
            return entity->should_remove();
        }), entities.end());
    }
}

