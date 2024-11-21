#include "scene.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"

#include <ranges>

namespace lotus
{
Scene::Scene(Engine* _engine) : engine(_engine) { component_runners = std::make_unique<Component::ComponentRunners>(engine); }

Task<> Scene::tick_all(time_point time, duration delta)
{
    auto entities_to_add = new_entities.getAll();
    entities.insert(entities.end(), entities_to_add.begin(), entities_to_add.end());
    co_await tick(time, delta);
    std::vector<decltype(entities)::value_type::element_type*> entities_p{entities.size()};
    std::ranges::transform(entities, entities_p.begin(), [](auto& c) { return c.get(); });

    co_await component_runners->run(time, delta);

    auto removed = std::ranges::partition(entities, [](auto& entity) { return !entity->should_remove(); });

    if (std::ranges::begin(removed) != std::ranges::end(removed))
    {
        for (const auto& e : removed)
        {
            component_runners->removeComponents(e.get());
        }
        entities.erase(std::ranges::begin(removed), std::ranges::end(removed));
    }
}
} // namespace lotus
