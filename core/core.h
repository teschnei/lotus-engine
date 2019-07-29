#pragma once
#include "worker_pool.h"
#include "config.h"
#include "renderer/renderer.h"
#include "entity/camera.h"
#include "input.h"
#include "types.h"

namespace lotus
{
    class Game;
    class Engine
    {
    public:
        Engine(const std::string& appname, uint32_t appVersion, Game* game);

        void run();
        void close() { closing = true; }

        Renderer renderer;
        Input input;
        Camera camera;
        WorkerPool worker_pool {this};
        Config config;
        Game* game;
        time_point simulation_time;

    private:
        void mainLoop();
        bool closing{ false };
    };
}
