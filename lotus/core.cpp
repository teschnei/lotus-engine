#include "core.h"
#include "game.h"
#include "light_manager.h"
#include "renderer/vulkan/hybrid/renderer_hybrid.h"
#include "renderer/vulkan/raster/renderer_rasterization.h"
#include "renderer/vulkan/raytrace/renderer_raytrace.h"
#include "renderer/vulkan/renderer.h"
#include "renderer/vulkan/window.h"
#include "ui/events.h"
#include "ui/ui.h"
#include "worker_pool.h"

namespace lotus
{
Engine::Engine(Game* _game, Settings settings, std::unique_ptr<Config> _config)
    : game(_game), config(std::move(_config)), settings(settings), simulation_time(sim_clock::now())
{
    if (config->renderer.render_mode == Config::Renderer::RenderMode::Rasterization)
    {
        renderer = std::make_unique<RendererRasterization>(this);
    }
    else if (config->renderer.render_mode == Config::Renderer::RenderMode::Raytrace)
    {
        renderer = std::make_unique<RendererRaytrace>(this);
    }
    else if (config->renderer.render_mode == Config::Renderer::RenderMode::Hybrid)
    {
        renderer = std::make_unique<RendererHybrid>(this);
    }

    audio = std::make_unique<AudioEngine>(this);
    input = std::make_unique<Input>(this, renderer->window->window);
    worker_pool = std::make_unique<WorkerPool>(this);
    lights = std::make_unique<LightManager>(this);
    events = std::make_unique<ui::Events>();
    ui = std::make_unique<ui::Manager>(this);
}

Task<> Engine::Init()
{
    co_await renderer->Init();
    co_await renderer->InitCommon();
    co_await ui->Init();
}

Engine::~Engine() { renderer->gpu->device->waitIdle(); }

void Engine::run()
{
    auto loop = mainLoop();
    worker_pool->Run();
}

WorkerTask<> Engine::mainLoop()
{
    try
    {
        co_await Init();
        co_await game->entry();
        while (!closing)
        {
            time_point new_sim_time = sim_clock::now();
            duration sim_delta = new_sim_time - simulation_time;
            simulation_time = new_sim_time;
            worker_pool->processFrameWaits();
            // make sure we're on the main thread for any SDL events
            co_await worker_pool->mainThread();
            input->GetInput();
            co_await game->tick_all(simulation_time, sim_delta);
            co_await renderer->drawFrame();
        }
    }
    catch (...)
    {
        worker_pool->Stop(std::current_exception());
    }
    worker_pool->Stop();
}
} // namespace lotus
