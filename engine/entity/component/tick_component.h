#pragma once

#include <functional>
#include "engine/entity/component/component.h"

namespace lotus
{
    class TickComponent : public Component
    {
    public:
        explicit TickComponent(Entity* entity, Engine* engine, std::function<void(time_point, duration)> function) : Component(entity, engine), tick_function(function) {}
        virtual Task<> tick(time_point time, duration delta)
        {
            tick_function(time, delta);
            co_return;
        };
    private:
        std::function<void(time_point, duration)> tick_function;
    };
}
