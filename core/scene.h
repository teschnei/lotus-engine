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
        void AddEntity(std::shared_ptr<Entity>&& entity);
    protected:
        Engine* engine;
        std::shared_ptr<TopLevelAccelerationStructure> top_level_as;
        bool rebuild_as{ false };
        std::vector<std::shared_ptr<Entity>> entities;
    };
}
