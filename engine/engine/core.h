#pragma once
#include "worker_pool.h"
#include "config.h"
#include "renderer/vulkan/renderer.h"
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
        struct Settings
        {
            std::string app_name;
            uint32_t app_version;
            Renderer::Settings renderer_settings;
        };
        Engine(Game* game, Settings settings, std::unique_ptr<Config> config);
        ~Engine();

        void run();
        void close() { closing = true; }

        Game* game;
        std::unique_ptr<Config> config;
        Settings settings;
        Renderer renderer;
        Input input;
        Camera* camera {nullptr};
        WorkerPool worker_pool{ this };
        LightManager lights{ this };

        void set_camera(Camera* _camera) { camera = _camera; }
        time_point getSimulationTime() { return simulation_time; }

    private:
        time_point simulation_time;
        void mainLoop();
        bool closing{ false };
    };
}
