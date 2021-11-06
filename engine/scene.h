#pragma once
#include "entity/entity.h"
#include <memory>
#include <vector>
#include "task.h"
#include "engine/core.h"
#include "engine/worker_pool.h"
#include "renderer/acceleration_structure.h"
#include "entity/component/component.h"

namespace lotus
{
    class Engine;
    //class Scene;

    //template<typename T, typename... Args>
    //concept EntityInitializer = requires(T t, Engine* engine, Scene* scene, Args... args) { { T::Init(engine, scene, args...); } -> std::is_convertible_to<std::shared_ptr<Entity>> };

    class Scene
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
            entities.emplace_back(sp);
            co_return std::make_pair(sp, components);
        }
        template<typename F>
        void forEachEntity(F func)
        {
            for(auto& entity : entities)
            {
                func(entity);
            }
        }
        std::unique_ptr<Component::ComponentRunners> component_runners;
    protected:
        virtual Task<> tick(time_point time, duration delta) { co_return; }

        Engine* engine;
        std::vector<std::shared_ptr<Entity>> entities;
    };
}
