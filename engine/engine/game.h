#pragma once
#include <memory>
#include "types.h"
#include "scene.h"
#include "core.h"

namespace lotus
{
    class Game
    {
    public:
        Game(const Engine::Settings& settings) : engine(std::make_unique<Engine>(this, settings)) {}
        virtual ~Game() = default;

        virtual void run() { engine->run(); }
        virtual void tick(time_point time, duration delta) = 0;

        std::unique_ptr<Scene> scene;
        std::unique_ptr<Engine> engine;
    };
}
