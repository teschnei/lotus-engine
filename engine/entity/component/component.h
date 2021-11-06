#pragma once

#include <vector>
#include <ranges>
#include <concepts>
#include <map>
#include <unordered_map>
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

        template<typename T, typename... Deps>
        class Component
        {
        public:
            Component(const Component&) = delete;
            Component(Component&&) = default;
            Component& operator=(const Component&) = delete;
            Component& operator=(Component&&) = default;
            static constexpr int priority = (std::max({ (Deps::priority)..., 0 }) + 1);
            static uint32_t TypeID() { return IDGenerator<ComponentRunners, uint32_t>::template GetNewID<T>(); }
        protected:
            Component(Entity* _entity, Engine* _engine, Deps&... deps) : entity(_entity), engine(_engine), dependencies(deps...) {}
            Entity* entity;
            Engine* engine;
            std::tuple<Deps&...> dependencies;
        };

        template<typename T>
        concept ComponentConcept = requires(T t) { t.tick(time_point{}, duration{}); };

        template<typename T>
        concept ComponentInitConcept = ComponentConcept<T> && requires(T t) { t.init(); };

        template<typename T>
        concept ComponentInputConcept = ComponentConcept<T> && requires(T t, Input * i, const SDL_Event & e) { { t.handleInput(i, e) } -> std::convertible_to<bool>; };

        class ComponentRunners
        {
        public:
            explicit ComponentRunners(Engine* _engine) : engine(_engine) {}

            template<ComponentInitConcept T, typename... Args>
            Task<T*> addComponent(Entity* entity, Args&&... args)
            {
                auto& runner = component_runners[T::priority][T::TypeID()];
                if (runner)
                    co_return co_await static_cast<ComponentRunner<T>*>(runner.get())->addComponentInit(entity, engine, std::forward<Args>(args)...);
                throw std::invalid_argument(std::format("Component (ID: {}) was added but not registered", T::TypeID()));
            }

            template<ComponentConcept T, typename... Args>
            T* addComponent(Entity* entity, Args&&... args)
            {
                auto& runner = component_runners[T::priority][T::TypeID()];
                if (runner)
                    return static_cast<ComponentRunner<T>*>(runner.get())->addComponent(entity, engine, std::forward<Args>(args)...);
                throw std::invalid_argument(std::format("Component (ID: {}) was added but not registered", T::TypeID()));
            }

            Task<> run(time_point time, duration elapsed)
            {
                for (const auto& [priority, runners] : component_runners)
                {
                    std::vector<Task<>> tasks;
                    tasks.reserve(runners.size());
                    std::ranges::for_each(runners, [&tasks, time, elapsed](auto& c) { tasks.push_back(c.second->run(time, elapsed)); });
                    for (auto& task : tasks)
                    {
                        co_await task;
                    };
                }
            }

            void handleInput(Input* input, const SDL_Event& event)
            {
                for (auto& c : component_input_runners)
                {
                    if (c->handleInput(input, event))
                        return;
                }
            }

            template<ComponentConcept T>
            void registerComponent()
            {
                auto new_runner = std::make_unique<ComponentRunner<T>>();
                if constexpr (ComponentInputConcept<T>)
                    component_input_runners.push_back(new_runner.get());
                component_runners[T::priority][T::TypeID()] = std::move(new_runner);
            }

        private:
            class ComponentRunnerInterface
            {
            public:
                virtual Task<> run(time_point time, duration elapsed) = 0;
                virtual bool handleInput(Input* input, const SDL_Event& event) = 0;
            };

            template<ComponentConcept T>
            class ComponentRunner : public ComponentRunnerInterface
            {
            public:
                template<typename... Args>
                T* addComponent(Args&&... args)
                {
                    auto component = std::make_unique<T>(std::forward<Args>(args)...);
                    auto component_ref = component.get();
                    new_components.queue(std::move(component));
                    return component_ref;
                }
                template<typename... Args>
                Task<T*> addComponentInit(Args&&... args)
                {
                    auto component = std::make_unique<T>(std::forward<Args>(args)...);
                    auto component_ref = component.get();
                    co_await component->init();
                    new_components.queue(std::move(component));
                    co_return component_ref;
                }
                virtual Task<> run(time_point time, duration elapsed) override
                {
                    auto components_to_add = new_components.getAll();
                    std::ranges::move(components_to_add, std::back_inserter(components));
                    std::vector<decltype(std::declval<T>().tick(time_point{}, duration{})) > tasks;
                    tasks.reserve(components.size());
                    std::ranges::for_each(components, [&tasks, time, elapsed](auto& c) { tasks.push_back(c->tick(time, elapsed)); });
                    for (auto& task : tasks)
                    {
                        co_await task;
                    };
                }
                virtual bool handleInput(Input* input, const SDL_Event& event) override
                {
                    if constexpr (ComponentInputConcept<T>)
                    {
                        for (auto& c : components)
                        {
                            if (c->handleInput(input, event))
                                return true;
                        }
                    }
                    return false;
                }
            private:
                SharedLinkedList<std::unique_ptr<T>> new_components;
                std::vector<std::unique_ptr<T>> components;
            };

            std::map<int, std::unordered_map<uint32_t, std::unique_ptr<ComponentRunnerInterface>>> component_runners;
            std::vector<ComponentRunnerInterface*> component_input_runners;
            Engine* engine;
        };
    }
}