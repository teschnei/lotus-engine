#pragma once

#include <functional>
#include "engine/entity/component/component.h"

namespace lotus
{
    class TickComponent : public Component
    {
    public:
        explicit TickComponent(Entity* entity, Engine* engine, std::function<bool(Engine*, time_point, duration)> tick, std::function<void(Engine*)> cleanup = {}) : Component(entity, engine), tick_function(tick), cleanup_function(cleanup) {}
        virtual ~TickComponent()
        {
            if (cleanup_function)
                cleanup_function(engine);
        }
        virtual Task<> tick(time_point time, duration delta)
        {
            if (tick_function(engine, time, delta))
                remove = true;
            co_return;
        };
    private:
        std::function<bool(Engine*, time_point, duration)> tick_function;
        std::function<void(Engine*)> cleanup_function;
    };
}
