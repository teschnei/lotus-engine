#include <SDL2/SDL.h>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/entity/renderable_entity.h"
#include "engine/scene.h"
#include "entity/landscape_entity.h"
#include "entity/actor.h"
#include "test_loader.h"
#include "dat/mmb.h"
#include "dat/os2.h"
#include "dat/d3m.h"
#include "engine/entity/free_flying_camera.h"
#include "engine/entity/component/camera_cascades_component.h"
#include "entity/component/third_person_ffxi_entity_input.h"
#include "entity/third_person_ffxi_camera.h"
#include "config.h"
#include "engine/ui/element.h"

#include "dat/dat_parser.h"
#include "particle_tester.h"

#include <iostream>

class Game : public lotus::Game
{
public:
    Game(const lotus::Settings& settings) : lotus::Game(settings, std::make_unique<FFXIConfig>()) {}

    virtual lotus::Task<> entry() override
    {
        engine->lights->light.diffuse_dir = glm::normalize(-glm::vec3{ -25.f, -100.f, -50.f });
        auto ele = std::make_shared<lotus::ui::Element>();
        ele->SetPos({20, -20});
        ele->SetHeight(300);
        ele->SetWidth(700);
        ele->anchor = lotus::ui::Element::AnchorPoint::BottomLeft;
        ele->parent_anchor = lotus::ui::Element::AnchorPoint::BottomLeft;
        ele->bg_colour = glm::vec4{0.f, 0.f, 0.f, 0.4f};
        co_await engine->ui->addElement(ele);

        auto ele2 = std::make_shared<lotus::ui::Element>();
        ele2->SetPos({10, -10});
        ele2->SetHeight(30);
        ele2->SetWidth(ele->GetWidth() - 20);
        ele2->anchor = lotus::ui::Element::AnchorPoint::BottomLeft;
        ele2->parent_anchor = lotus::ui::Element::AnchorPoint::BottomLeft;
        ele2->bg_colour = glm::vec4{0.f, 0.f, 0.f, 0.7f};
        co_await engine->ui->addElement(ele2, ele);

        engine->worker_pool->background(load_scene());
    }
    virtual lotus::Task<> tick(lotus::time_point, lotus::duration) override
    {
        co_return;
    }
private:
    std::shared_ptr<lotus::Texture> default_texture;
    std::unique_ptr<lotus::Scene> loading_scene;

    lotus::WorkerTask<> load_scene()
    {
        loading_scene = std::make_unique<lotus::Scene>(engine.get());
        auto default_texture_work = lotus::Texture::LoadTexture<TestTextureLoader>(engine.get(), "default");
        default_texture = co_await default_texture_work;
        auto path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
        /* zone dats vtable:
        (i < 256 ? i + 100  : i + 83635) // Model
        (i < 256 ? i + 5820 : i + 84735) // Dialog
        (i < 256 ? i + 6420 : i + 85335) // Actor
        (i < 256 ? i + 6720 : i + 86235) // Event
        */
        auto landscape = co_await loading_scene->AddEntity<FFXILandscapeEntity>(path / "ROM/342/73.DAT");
        //costumeid 3111 (arciela 3074)
        //auto player = scene->AddEntity<Actor>(path / "/ROM/310/3.DAT");
        auto player = co_await loading_scene->AddEntity<Actor>(path / "ROM/309/105.DAT");
        player->setPos(glm::vec3(259.f, -87.f, 99.f));
        auto camera = co_await loading_scene->AddEntity<ThirdPersonFFXICamera>(std::weak_ptr<lotus::Entity>(player));
        if (engine->config->renderer.render_mode == lotus::Config::Renderer::RenderMode::Rasterization)
        {
            co_await camera->addComponent<lotus::CameraCascadesComponent>();
        }
        engine->set_camera(camera.get());
        engine->camera->setPerspective(glm::radians(70.f), engine->renderer->swapchain->extent.width / (float)engine->renderer->swapchain->extent.height, 0.01f, 1000.f);

        co_await player->addComponent<ThirdPersonEntityFFXIInputComponent>(engine->input.get());
        co_await player->addComponent<ParticleTester>(engine->input.get());

        co_await update_scene(std::move(loading_scene));
    }
};

int main(int argc, char* argv[]) {

    lotus::Settings settings;
    settings.app_name = "core-test";
    settings.app_version = VK_MAKE_VERSION(1, 0, 0);
    Game game{settings};

    game.run();
    return EXIT_SUCCESS;
}
