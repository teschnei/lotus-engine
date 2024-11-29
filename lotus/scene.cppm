module;

#include <chrono>
#include <coroutine>
#include <memory>
#include <vector>

export module lotus:core.scene;

import :entity;
import :entity.component;
import :util;

namespace lotus
{
class Engine;
// class Scene;

// template<typename T, typename... Args>
// concept EntityInitializer = requires(T t, Engine* engine, Scene* scene, Args... args) { { T::Init(engine, scene,
// args...); } -> std::is_convertible_to<std::shared_ptr<Entity>> };

export class Scene
{
public:
    explicit Scene(Engine* _engine);
    Task<> tick_all(time_point time, duration delta);

    template <typename T, typename... Args>
    [[nodiscard("Work must be awaited to be processed")]]
    auto AddEntity(Args... args) -> decltype(T::Init(std::declval<Engine*>(), this, args...))
    {
        auto [sp, components] = co_await T::Init(engine, this, args...);
        sp->setSharedPtr(sp);
        new_entities.queue(sp);
        co_return std::make_pair(sp, components);
    }
    template <typename F> void forEachEntity(F func)
    {
        for (auto& entity : entities)
        {
            func(entity);
        }
    }

    template <typename T, typename... Args> std::tuple<typename T::pointer, typename Args::pointer...> AddComponents(T&& arg, Args&&... args)
    {
        typename T::pointer c = nullptr;
        if (arg)
            c = component_runners->addComponent(std::move(arg));
        return std::tuple_cat(std::tie(c), AddComponents(std::forward<Args>(args)...));
    }

    template <typename T> std::tuple<typename T::pointer> AddComponents(T&& arg)
    {
        if (arg)
            return component_runners->addComponent(std::move(arg));
        return nullptr;
    }
    std::unique_ptr<Component::ComponentRunners> component_runners;

protected:
    virtual Task<> tick(time_point time, duration delta) { co_return; }

    Engine* engine;
    std::vector<std::shared_ptr<Entity>> entities;
    lotus::SharedLinkedList<std::shared_ptr<Entity>> new_entities;
};
} // namespace lotus
