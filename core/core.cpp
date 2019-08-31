#include "core.h"
#include "game.h"

namespace lotus
{
    Engine::Engine(const std::string& appname, uint32_t appVersion, Game* _game) : game(_game), renderer(this, appname, appVersion), input(this, renderer.window), camera(this, &input)
    {
        renderer.generateCommandBuffers();
    }

    Engine::~Engine()
    {
        renderer.device->waitIdle(renderer.dispatch);
    }

    void Engine::run()
    {
        mainLoop();
    }

    void Engine::mainLoop()
    {
        while (!closing)
        {
            time_point new_sim_time = sim_clock::now();
            duration sim_delta = new_sim_time - simulation_time;
            simulation_time = new_sim_time;
            input.GetInput();
            game->tick(simulation_time, sim_delta);
            camera.tick_all(simulation_time, sim_delta);
            renderer.drawFrame();
        }
    }
}
