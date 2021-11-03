#include "scene.h"
#include "entity/renderable_entity.h"
#include "entity/deformable_entity.h"
#include "entity/particle.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"
#include "renderer/vulkan/renderer_raytrace_base.h"

#include <ranges>

namespace lotus
{
    Scene::Scene(Engine* _engine) : engine(_engine)
    {
        component_runners = std::make_unique<Test::ComponentRunners>(engine);
    }

    Task<> Scene::render()
    {
        engine->renderer->resources->Reset();
        uint32_t image_index = engine->renderer->getCurrentImage();
        std::vector<Task<>> tasks;
        for (auto& entity : entities)
        {
            tasks.push_back(entity->render_all(engine, entity));
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
        std::ranges::transform(entities, entities_p.begin(), [](auto& c) { return c.get(); });

        //start all tick tasks before awaiting any
        std::vector<Task<>> tick_tasks;
        tick_tasks.reserve(entities_p.size());
        std::ranges::for_each(entities_p, [time, delta, &tick_tasks](auto p) { tick_tasks.push_back(p->tick_all(time, delta)); });

        for (auto& t : tick_tasks)
        {
            co_await t;
        }

        auto removed = std::ranges::partition(entities, [](auto& entity)
        {
            return !entity->should_remove();
        });
        if (std::ranges::begin(removed) != std::ranges::end(removed))
        {
            //remove the entities from the entity list, but keep them alive at least until their command buffers are no longer in use
            std::vector<decltype(entities)::value_type> removed_entities(std::make_move_iterator(std::ranges::begin(removed)), std::make_move_iterator(std::ranges::end(removed)));
            entities.erase(std::ranges::begin(removed), std::ranges::end(removed));
            engine->worker_pool->gpuResource(std::move(removed_entities));
        }

        //TODO: move this back before queries
        auto component_task = component_runners->run(time, delta);
        engine->renderer->runRaytracerQueries();
        co_await component_task;
    }
}

