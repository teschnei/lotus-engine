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
        Game(const Engine::Settings& settings, std::unique_ptr<Config> config) : engine(std::make_unique<Engine>(this, settings, std::move(config))) {}
        virtual ~Game() = default;

        virtual void run() { engine->run(); }
        void tick_all(time_point time, duration delta)
        {
            tick(time, delta);
            scene->tick_all(time, delta);
        }
        std::unique_ptr<Engine> engine;
        std::unique_ptr<Scene> scene;

    protected:
        virtual void tick(time_point time, duration delta) {}
    };
}
