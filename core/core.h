#pragma once
#include "worker_pool.h"
#include "config.h"
#include "renderer/renderer.h"
#include "entity/camera.h"
#include "input.h"
#include "types.h"
#include "light_manager.h"

namespace lotus
{
    class Game;
    class Engine
    {
    public:
        Engine(const std::string& appname, uint32_t appVersion, Game* game);
        ~Engine();

        void run();
        void close() { closing = true; }

        Game* game;
        Renderer renderer;
        Input input;
        Camera camera;
        WorkerPool worker_pool{ this };
        Config config;
        LightManager lights{ this };
        time_point simulation_time;

    private:
        void mainLoop();
        bool closing{ false };
    };
}
