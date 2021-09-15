#pragma once
#include <memory>
#include <vector>
#include "engine/types.h"
#include "engine/task.h"

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

        Task<> init() { co_return; }
        Task<> tick_all(time_point time, duration delta);
        Task<> render_all(Engine* engine, std::shared_ptr<Entity>& sp);
        bool removed() { return remove; }

        template<typename T, typename... Args> requires std::derived_from<T, Component>
        Task<T*> addComponent(Args&&... args)
        {
            auto component = std::make_unique<T>(entity, engine, std::forward<Args>(args)...);
            co_await component->init();
            auto comp_ptr = component.get();
            components.push_back(std::move(component));
            co_return comp_ptr;
        };

        template<typename T> requires std::derived_from<T, Component>
        T* getComponent()
        {
            for (const auto& component : components)
            {
                if (auto cast = dynamic_cast<T*>(component.get()))
                {
                    return cast;
                }
            }
            return nullptr;
        }

        template<typename T> requires std::predicate<T, Component*>
        void removeComponent(T func)
        {
            for (const auto& component : components)
            {
                if (func(component.get()))
                {
                    component->remove = true;
                }
            }
        }

    protected:
        virtual Task<> tick(time_point time, duration delta) { co_return; };
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) { co_return; };

        Entity* entity;
        Engine* engine;
        bool remove{ false };
        bool componentsEmpty() { return components.empty(); }
    private:
        std::vector<std::unique_ptr<Component>> components;
    };
}
