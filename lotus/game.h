#pragma once
#include <memory>
#include "types.h"
#include "scene.h"
#include "core.h"
#include "worker_task.h"

namespace lotus
{
    class Game
    {
    public:
        Game(const Settings& settings, std::unique_ptr<Config> config) : engine(std::make_unique<Engine>(this, settings, std::move(config))) {}
        virtual ~Game() = default;

        virtual Task<> entry() = 0;
        void run() { engine->run(); }
        Task<> tick_all(time_point time, duration delta)
        {
            co_await tick(time, delta);
            if (scene)
                co_await scene->tick_all(time, delta);
        }
        std::unique_ptr<Engine> engine;
        std::unique_ptr<Scene> scene;
        Task<> update_scene(std::unique_ptr<Scene>&& scene);

    protected:
        virtual Task<> tick(time_point time, duration delta) { co_return; }
    };
}
