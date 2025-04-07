module;

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

export module lotus:core.engine;

import :audio.engine;
import :core.config;
import :core.input;
import :renderer.vulkan.settings;
import :util;

namespace lotus
{
export struct Settings
{
    std::string app_name;
    uint32_t app_version;
    RendererSettings renderer_settings;
};

export class Game;
class LightManager;
class WorkerPool;
class Renderer;
namespace ui
{
class Events;
class Manager;
} // namespace ui
export namespace Component
{
class CameraComponent;
}

export class Engine
{
public:
    Engine(Game* game, Settings settings, std::unique_ptr<Config> config);
    ~Engine();

    Task<> Init();

    void run();
    void close() { closing = true; }

    Game* game;
    std::unique_ptr<Config> config;
    Settings settings;
    std::unique_ptr<Input> input;
    Component::CameraComponent* camera{nullptr};
    std::unique_ptr<AudioEngine> audio;
    std::unique_ptr<WorkerPool> worker_pool;
    std::unique_ptr<LightManager> lights;
    std::unique_ptr<ui::Events> events;
    std::unique_ptr<ui::Manager> ui;
    std::unique_ptr<Renderer> renderer;

    void set_camera(Component::CameraComponent* _camera) { camera = _camera; }
    time_point getSimulationTime() { return simulation_time; }

private:
    time_point simulation_time;
    WorkerTask<> mainLoop();
    bool closing{false};
};
} // namespace lotus
