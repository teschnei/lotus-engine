#pragma once

namespace lotus {
    class Entity;

    class Component
    {
    public:
        explicit Component(Entity* entity) : entity(entity) {}
        Component(const Component&) = delete;
        Component(Component&&) = default;
        Component& operator=(const Component&) = delete;
        Component& operator=(Component&&) = default;
        virtual ~Component() = default;
        virtual void tick() = 0;
    protected:
        Entity* entity;
    };
}
