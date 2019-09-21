#pragma once
#include "entity/entity.h"
#include <memory>
#include <vector>
#include "renderer/acceleration_structure.h"

namespace lotus
{
    class Engine;

    class Scene
    {
    public:
        Scene(Engine* _engine) : engine(_engine) {}
        void render();
        template <typename T, typename... Args>
        std::shared_ptr<T> AddEntity(Args... args)
        {
            auto sp = std::static_pointer_cast<T>(entities.emplace_back(std::make_shared<T>()));
            sp->Init(sp, engine, args...);
            RebuildTLAS();
            return sp;
        }
    protected:
        void RebuildTLAS();

        Engine* engine;
        std::shared_ptr<TopLevelAccelerationStructure> top_level_as;
        bool rebuild_as{ false };
        std::vector<std::shared_ptr<Entity>> entities;
    };
}
