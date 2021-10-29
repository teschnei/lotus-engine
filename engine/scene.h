#pragma once
#include "entity/entity.h"
#include <memory>
#include <vector>
#include "task.h"
#include "engine/core.h"
#include "engine/worker_pool.h"
#include "renderer/acceleration_structure.h"
#include "entity/component/component_rewrite_test/component.h"

namespace lotus
{
    class Engine;

    class Scene
    {
    public:
        explicit Scene(Engine* _engine);
        Task<> render();
        Task<> tick_all(time_point time, duration delta);

        template <typename T, typename... Args>
        [[nodiscard("Work must be awaited to be processed")]]
        Task<std::shared_ptr<T>> AddEntity(Args... args) requires std::derived_from<T, Entity>
        {
            auto sp = co_await T::Init(engine, args...);
            sp->setSharedPtr(sp);
            entities.emplace_back(sp);
            co_return sp;
        }
        template<typename F>
        void forEachEntity(F func)
        {
            for(auto& entity : entities)
            {
                func(entity);
            }
        }
        std::unique_ptr<Test::ComponentRunners> component_runners;
    protected:
        virtual Task<> tick(time_point time, duration delta) { co_return; }

        Engine* engine;
        std::vector<std::shared_ptr<Entity>> entities;
    };
}
