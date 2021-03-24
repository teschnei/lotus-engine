#include "component.h"

#include <algorithm>

namespace lotus
{
    Task<> Component::tick_all(time_point time, duration delta)
    {
        std::vector<decltype(components)::value_type::element_type*> components_p{components.size()};
        std::ranges::transform(components, components_p.begin(), [](auto& c) { return c.get(); });
        for (auto component : components_p)
        {
            co_await component->tick_all(time, delta);
        }
        components.erase(std::remove_if(components.begin(), components.end(), [](auto& component)
        {
            return component->removed();
        }), components.end());
        co_await tick(time, delta);
    }

    Task<> Component::render_all(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        for (auto& component : components)
        {
            co_await component->render_all(engine, sp);
        }
        co_await render(engine, sp);
    }
}