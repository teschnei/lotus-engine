#include <SDL.h>

#include "core/core.h"
#include "core/game.h"
#include "core/entity/renderable_entity.h"
#include "core/task/renderable_entity_init.h"
#include "core/scene.h"
#include "entity/landscape_entity.h"
#include "task/landscape_dat_load.h"

class Game : public lotus::Game
{
public:
    Game(const std::string appname, uint32_t appversion) : lotus::Game(appname, appversion)
    {
        scene = std::make_unique<lotus::Scene>(engine.get());
        scene->AddEntity<FFXILandscapeEntity>(R"(E:\Apps\SteamLibrary\SteamApps\common\ffxi\SquareEnix\FINAL FANTASY XI\ROM\342\73.dat)");
        engine->lights.directional_light.direction = glm::normalize(-glm::vec3{ -25.f, -100.f, -50.f });
        engine->lights.directional_light.color = glm::vec3{ 1.f, 1.f, 1.f };
        engine->lights.UpdateLightBuffer();
        engine->camera.setPerspective(glm::radians(70.f), engine->renderer.swapchain_extent.width / (float)engine->renderer.swapchain_extent.height, .5f, 400.f);
        engine->camera.setPos(glm::vec3(259.f, -90.f, 82.f));
    }
    virtual void tick(lotus::time_point time, lotus::duration delta) override
    {

    }
};

int main(int argc, char* argv[]) {

    Game game{ "core-test", VK_MAKE_VERSION(1, 0, 0) };

    game.run();

    return EXIT_SUCCESS;
}