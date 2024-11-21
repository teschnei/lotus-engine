#include "game.h"

namespace lotus
{
Task<> Game::update_scene(std::unique_ptr<Scene>&& next_scene)
{
    co_await engine->worker_pool->waitForFrame();
    scene = std::move(next_scene);
}
} // namespace lotus