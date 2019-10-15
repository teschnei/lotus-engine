#include <SDL.h>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/entity/renderable_entity.h"
#include "engine/task/renderable_entity_init.h"
#include "engine/scene.h"
#include "entity/landscape_entity.h"
#include "entity/actor.h"
#include "test_loader.h"
#include "dat/mmb.h"
#include "dat/os2.h"

class Game : public lotus::Game
{
public:
    Game(const lotus::Engine::Settings& settings) : lotus::Game(settings)
    {
        scene = std::make_unique<lotus::Scene>(engine.get());
        default_texture = lotus::Texture::LoadTexture<TestTextureLoader>(engine.get(), "default");
        //scene->AddEntity<FFXILandscapeEntity>(R"(E:\Apps\SteamLibrary\SteamApps\common\ffxi\SquareEnix\FINAL FANTASY XI\ROM\342\73.dat)");
        auto iroha = scene->AddEntity<Actor>(R"(E:\Apps\SteamLibrary\SteamApps\common\ffxi\SquareEnix\FINAL FANTASY XI\ROM\310\3.dat)");
        //iroha->setPos(glm::vec3(259.f, -99.f, 99.f));
        engine->lights.directional_light.direction = glm::normalize(-glm::vec3{ -25.f, -100.f, -50.f });
        engine->lights.directional_light.color = glm::vec3{ 1.f, 1.f, 1.f };
        engine->lights.UpdateLightBuffer();
        engine->camera.setPerspective(glm::radians(70.f), engine->renderer.swapchain_extent.width / (float)engine->renderer.swapchain_extent.height, .5f, 400.f);
        //engine->camera.setPos(glm::vec3(259.f, -90.f, 82.f));
        engine->camera.setPos(glm::vec3(5.f, -0.f, 0.f));
    }
    virtual void tick(lotus::time_point time, lotus::duration delta) override
    {

    }
    std::shared_ptr<lotus::Texture> default_texture;
};

int main(int argc, char* argv[]) {

    lotus::Engine::Settings settings;
    settings.app_name = "core-test";
    settings.app_version = VK_MAKE_VERSION(1, 0, 0);
    settings.renderer_settings.landscape_vertex_input_attribute_descriptions = FFXI::MMB::Vertex::getAttributeDescriptions();
    settings.renderer_settings.landscape_vertex_input_binding_descriptions = FFXI::MMB::Vertex::getBindingDescriptions();
    settings.renderer_settings.model_vertex_input_attribute_descriptions = FFXI::OS2::Vertex::getAttributeDescriptions();
    settings.renderer_settings.model_vertex_input_binding_descriptions = FFXI::OS2::Vertex::getBindingDescriptions();
    Game game{settings};

    game.run();

    return EXIT_SUCCESS;
}