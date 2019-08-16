#include <SDL.h>

#include "core/core.h"
#include "core/game.h"
#include "core/entity/renderable_entity.h"
#include "core/task/entity_render.h"
#include "core/task/renderable_entity_init.h"
#include "core/scene.h"
#include <iostream>
#include "ffxi/ffxi_load_land_test.h"

class Game : public lotus::Game
{
public:
    Game(const std::string appname, uint32_t appversion) : lotus::Game(appname, appversion)
    {
        cascade_data_ubo = engine->renderer.memory_manager->GetBuffer(sizeof(cascade_data) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        cascade_matrices_ubo = engine->renderer.memory_manager->GetBuffer(sizeof(cascade_matrices) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        FFXILoadLandTest test(this);
        auto entity = test.getLand();
        scene = std::make_unique<lotus::Scene>();
        scene->entities.push_back(std::shared_ptr<lotus::RenderableEntity>(entity));
        engine->camera.setPerspective(glm::radians(70.f), engine->renderer.swapchain_extent.width / (float)engine->renderer.swapchain_extent.height, .5f, 400.f);
        engine->camera.setPos(glm::vec3(259.f, -90.f, 82.f));
    }
    virtual void tick(lotus::time_point time, lotus::duration delta) override
    {

    }
};

int main(int argc, char* argv[]) {

    Game game{ "core-test", VK_MAKE_VERSION(1, 0, 0) };

    try {
        game.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}