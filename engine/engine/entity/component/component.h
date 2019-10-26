#pragma once
#include "../../types.h"

namespace lotus {
    class Entity;
    class Engine;

    class Component
    {
    public:
        explicit Component(Entity* entity) : entity(entity) {}
        Component(const Component&) = delete;
        Component(Component&&) = default;
        Component& operator=(const Component&) = delete;
        Component& operator=(Component&&) = default;
        virtual ~Component() = default;
        virtual void tick(time_point time, duration delta) {};
        virtual void render(Engine* engine) {};
    protected:
        Entity* entity;
    };
}
