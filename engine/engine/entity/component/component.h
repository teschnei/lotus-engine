#pragma once
#include <memory>
#include "engine/types.h"

namespace lotus {
    class Entity;
    class Engine;

    class Component
    {
    public:
        explicit Component(Entity* entity, Engine* engine) : entity(entity), engine(engine) {}
        Component(const Component&) = delete;
        Component(Component&&) = default;
        Component& operator=(const Component&) = delete;
        Component& operator=(Component&&) = default;
        virtual ~Component() = default;
        virtual void tick(time_point time, duration delta) {};
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp) {};
        bool removed() { return remove; }
    protected:
        Entity* entity;
        Engine* engine;
        bool remove{ false };
    };
}
