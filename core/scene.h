#pragma once
#include "entity/entity.h"
#include <memory>
#include <vector>

namespace lotus
{
    class Engine;

    class Scene
    {
    public:
        void render(Engine* engine);
        std::vector<std::shared_ptr<Entity>> entities;
    };
}
