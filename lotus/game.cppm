module;

#include <coroutine>
#include <memory>

export module lotus:core.game;

import :core.config;
import :core.engine;
import :core.scene;
import :util;

export namespace lotus
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

Task<> Game::update_scene(std::unique_ptr<Scene>&& next_scene)
{
    co_await engine->worker_pool->waitForFrame();
    scene = std::move(next_scene);
}
} // namespace lotus
