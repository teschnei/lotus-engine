#include "core.h"
#include "game.h"
#include "renderer/vulkan/window.h"
//#include "renderer/vulkan/hybrid/renderer_hybrid.h"
//#include "renderer/vulkan/raster/renderer_rasterization.h"
#include "renderer/vulkan/raytrace/renderer_raytrace.h"

namespace lotus
{
    Engine::Engine(Game* _game, Settings settings, std::unique_ptr<Config> _config) : game(_game), config(std::move(_config)), settings(settings), simulation_time(sim_clock::now())
    {
        if (config->renderer.render_mode == Config::Renderer::RenderMode::Rasterization)
        {
            //renderer = std::make_unique<RendererRasterization>(this);
        }
        else if (config->renderer.render_mode == Config::Renderer::RenderMode::Raytrace)
        {
            renderer = std::make_unique<RendererRaytrace>(this);
        }
        else if (config->renderer.render_mode == Config::Renderer::RenderMode::Hybrid)
        {
            //renderer = std::make_unique<RendererHybrid>(this);
        }

        input = std::make_unique<Input>(this, renderer->window->window);
        worker_pool = std::make_unique<WorkerPool>(this);
        lights = std::make_unique<LightManager>(this);

        renderer->Init();
    }

    Engine::~Engine()
    {
        renderer->gpu->device->waitIdle();
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
            input->GetInput();
            game->tick_all(simulation_time, sim_delta);
            renderer->drawFrame();
        }
    }
}
