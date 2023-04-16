#pragma once

#include <vector>
#include <ranges>
#include <concepts>
#include <map>
#include <mutex>
#include "engine/core.h"
#include "engine/idgenerator.h"
#include "engine/shared_linked_list.h"
#include "engine/types.h"
#include "engine/task.h"
#include "engine/renderer/sdl_inc.h"

namespace lotus
{
    class Engine;
    class Entity;
    class Input;

    namespace Component
    {
        class ComponentRunners;

        template<typename... Deps>
        struct Before
        {
            static consteval int priority() { return std::min({(Deps::priority)...}) - 1; }
        };
        template<typename... Deps>
        struct After
        {
            static consteval int priority() { return std::max({(Deps::priority)...}) + 1; }
        };
        struct Zero
        {
            static consteval int priority() { return 0; }
        };

        //technically should be some form of is_base_of but it doesn't work with crtp
        template<typename T>
        concept ComponentConcept = requires(T t) { T::priority; };

        template<typename T>
        concept ComponentTickConcept = ComponentConcept<T> && requires(T t) { t.tick(time_point{}, duration{}); };

        template<typename T>
        concept ComponentInitConcept = ComponentConcept<T> && requires(T t) { t.init(); };

        template<typename T>
        concept ComponentInputConcept = ComponentConcept<T> && requires(T t, Input* i, const SDL_Event& e) { { t.handleInput(i, e) } -> std::convertible_to<bool>; };

        template<typename T, typename Order = Zero>
        class Component
        {
        public:
            Component(const Component&) = delete;
            Component(Component&&) = default;
            Component& operator=(const Component&) = delete;
            Component& operator=(Component&&) = default;
            static constexpr int priority = Order::priority();
            static uint32_t TypeID() { return IDGenerator<ComponentRunners, uint32_t>::template GetNewID<T>(); }
            bool removed() { return _remove; }
            void remove() { _remove = true; }
            Entity* getEntity() { return entity; }
            template<typename... Args>
            static Task<std::unique_ptr<T>> make_component(Args&&... args)
            {
                auto c = std::make_unique<T>(std::forward<Args>(args)...);
                if constexpr (ComponentInitConcept<T>)
                {
                    co_await c->init();
                }
                co_return c;
            }
        protected:
            Component(Entity* _entity, Engine* _engine) : entity(_entity), engine(_engine) {}
            Entity* entity;
            Engine* engine;
            bool _remove{ false };
        };

        class ComponentRunners
        {
        public:
            explicit ComponentRunners(Engine* _engine) : engine(_engine) {}

            template<ComponentConcept T>
            T* addComponent(std::unique_ptr<T>&& com)
            {
                auto& runner = component_runners[T::priority][T::TypeID()];
                if (!runner)
                {
                    std::lock_guard lg(runner_add_mutex);
                    if (!runner)
                    {
                        runner = std::make_unique<ComponentRunner<T>>();
                        if constexpr (ComponentInputConcept<T>)
                            component_input_runners.push_back(runner.get());
                    }
                }
                return static_cast<ComponentRunner<T>*>(runner.get())->addComponent(std::move(com));
            }

            void removeComponents(Entity* entity)
            {
                for (const auto& [priority, runners] : component_runners)
                {
                    for (const auto& [id, runner] : runners)
                    {
                        runner->remove(entity);
                    }
                }
            }

            Task<> run(time_point time, duration elapsed)
            {
                for (const auto& [priority, runners] : component_runners)
                {
                    std::ranges::for_each(runners, [] (auto& c) {
                        c.second->move_new_components();
                    });
                }
                for (const auto& [priority, runners] : component_runners)
                {
                    std::vector<Task<>> tasks;
                    tasks.reserve(runners.size());
                    std::ranges::for_each(runners, [&tasks, time, elapsed, this](auto& c) {
                        tasks.push_back(c.second->run(engine, time, elapsed));
                    });
                    for (auto& task : tasks)
                    {
                        co_await task;
                    };
                }
            }

            template<ComponentConcept T>
            T* getComponent(Entity* entity)
            {
                auto& runner = component_runners[T::priority][T::TypeID()];
                return static_cast<ComponentRunner<T>*>(runner.get())->getComponent(entity);
            }


            void handleInput(Input* input, const SDL_Event& event)
            {
                for (auto& c : component_input_runners)
                {
                    if (c->handleInput(input, event))
                        return;
                }
            }

        private:
            class ComponentRunnerInterface
            {
            public:
                virtual void move_new_components() = 0;
                virtual Task<> run(Engine* engine, time_point time, duration elapsed) = 0;
                virtual void remove(Entity* entity) = 0;
                virtual bool handleInput(Input* input, const SDL_Event& event) = 0;
            };

            template<ComponentConcept T>
            class ComponentRunner : public ComponentRunnerInterface
            {
            public:
                T* addComponent(std::unique_ptr<T>&& component)
                {
                    auto component_ref = component.get();
                    new_components.queue(std::move(component));
                    return component_ref;
                }
                virtual void move_new_components() override
                {
                    auto components_to_add = new_components.getAll();
                    std::ranges::move(components_to_add, std::back_inserter(components));
                }
                virtual Task<> run(Engine* engine, time_point time, duration elapsed) override
                {
                    if constexpr (ComponentTickConcept<T>)
                    {
                        std::vector<decltype(std::declval<T>().tick(time_point{}, duration{})) > tasks;
                        tasks.reserve(components.size());
                        std::ranges::for_each(components, [&tasks, time, elapsed](auto& c) { if (!c->removed()) tasks.push_back(c->tick(time, elapsed)); });
                        for (auto& task : tasks)
                        {
                            co_await task;
                        };
                    }
                    auto part = std::ranges::partition(components, [](auto& c) { return !c->removed(); });
                    if (std::ranges::begin(part) != std::ranges::end(part))
                    {
                        std::vector<typename decltype(components)::value_type> removed_elements(std::make_move_iterator(std::ranges::begin(part)), std::make_move_iterator(std::ranges::end(part)));
                        components.erase(std::ranges::begin(part), std::ranges::end(part));
                        engine->worker_pool->gpuResource(std::move(removed_elements));
                    }
                    co_return;
                }
                virtual void remove(Entity* entity) override
                {
                    for (auto& c : components)
                    {
                        if (c->getEntity() == entity)
                        {
                            c->remove();
                        }
                    }
                }
                virtual bool handleInput(Input* input, const SDL_Event& event) override
                {
                    if constexpr (ComponentInputConcept<T>)
                    {
                        for (auto& c : components)
                        {
                            if (!c->removed() && c->handleInput(input, event))
                                return true;
                        }
                    }
                    return false;
                }
                T* getComponent(Entity* entity)
                {
                    auto c = std::ranges::find_if(components, [entity](auto& c) { return c->getEntity() == entity; });
                    if (c != components.end())
                        return c->get();
                    return nullptr;
                }
            private:
                SharedLinkedList<std::unique_ptr<T>> new_components;
                std::vector<std::unique_ptr<T>> components;
            };

            std::mutex runner_add_mutex;
            std::map<int, std::map<uint32_t, std::unique_ptr<ComponentRunnerInterface>>> component_runners;
            std::vector<ComponentRunnerInterface*> component_input_runners;
            Engine* engine;
        };
    }
}