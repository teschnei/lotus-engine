#include "scene.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"

#include <ranges>

namespace lotus
{
    Scene::Scene(Engine* _engine) : engine(_engine)
    {
        component_runners = std::make_unique<Component::ComponentRunners>(engine);
    }

    Task<> Scene::tick_all(time_point time, duration delta)
    {
        engine->renderer->resources->Reset();
        co_await tick(time, delta);
        std::vector<decltype(entities)::value_type::element_type*> entities_p{entities.size()};
        std::ranges::transform(entities, entities_p.begin(), [](auto& c) { return c.get(); });

        auto removed = std::ranges::partition(entities, [](auto& entity)
        {
            return !entity->should_remove();
        });
        auto component_task = component_runners->run(time, delta);
        co_await component_task;

        if (std::ranges::begin(removed) != std::ranges::end(removed))
        {
            //remove the entities from the entity list, but keep them alive at least until their command buffers are no longer in use
            std::vector<decltype(entities)::value_type> removed_entities(std::make_move_iterator(std::ranges::begin(removed)), std::make_move_iterator(std::ranges::end(removed)));
            entities.erase(std::ranges::begin(removed), std::ranges::end(removed));
            engine->worker_pool->gpuResource(std::move(removed_entities));
        }
    }
}

