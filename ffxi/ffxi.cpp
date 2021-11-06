#include "ffxi.h"

#include "test_loader.h"
#include "particle_tester.h"
#include "audio/ffxi_audio.h"
#include "entity/landscape_entity.h"
#include "entity/actor.h"
//#include "entity/component/third_person_ffxi_entity_input.h"
//#include "entity/component/third_person_ffxiv_entity_input.h"
//#include "entity/component/equipment_test_component.h"
#include "entity/third_person_ffxi_camera.h"
#include "entity/third_person_ffxiv_camera.h"

#include "engine/scene.h"
#include "engine/ui/element.h"
//#include "engine/entity/component/camera_cascades_component.h"

//#include "engine/entity/component/animation_component.h"
#include "engine/entity/component/deformable_raster_component.h"
#include "engine/entity/component/deformable_raytrace_component.h"

#include "engine/entity/component/instanced_raster_component.h"
#include "engine/entity/component/instanced_raytrace_component.h"
#include "engine/entity/component/static_collision_component.h"

#include "engine/entity/component/camera_component.h"
#include "entity/component/camera_third_person_component.h"

#include "entity/component/actor_component.h"
#include "entity/component/modern_third_person_input_component.h"
#include "entity/component/landscape_component.h"

#include <iostream>

FFXIGame::FFXIGame(const lotus::Settings& settings) : lotus::Game(settings, std::make_unique<FFXIConfig>()),
    dat_loader(std::make_unique<FFXI::DatLoader>(static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path)),
    audio(std::make_unique<FFXI::Audio>(engine.get()))
{
}

lotus::Task<> FFXIGame::entry() 
{
    default_texture = co_await lotus::Texture::LoadTexture("default", TestTextureLoader::LoadTexture, engine.get());

    system_dat = co_await SystemDat::Load(this);

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

lotus::Task<> FFXIGame::tick(lotus::time_point, lotus::duration)
{
    co_return;
}

lotus::WorkerTask<> FFXIGame::load_scene()
{
    loading_scene = std::make_unique<lotus::Scene>(engine.get());
    loading_scene->component_runners->registerComponent<lotus::Component::AnimationComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::PhysicsComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::DeformedMeshComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::DeformableRasterComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::DeformableRaytraceComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::InstancedModelsComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::InstancedRasterComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::InstancedRaytraceComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::StaticCollisionComponent>();
    loading_scene->component_runners->registerComponent<lotus::Component::CameraComponent>();
    loading_scene->component_runners->registerComponent<FFXI::CameraThirdPersonComponent>();
    loading_scene->component_runners->registerComponent<FFXI::ActorComponent>();
    loading_scene->component_runners->registerComponent<FFXI::ModernThirdPersonInputComponent>();
    loading_scene->component_runners->registerComponent<FFXI::ActorPCModelsComponent>();
    loading_scene->component_runners->registerComponent<FFXI::LandscapeComponent>();

    auto path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
    /* zone dats vtable:
    (i < 256 ? i + 100  : i + 83635) // Model
    (i < 256 ? i + 5820 : i + 84735) // Dialog
    (i < 256 ? i + 6420 : i + 85335) // Actor
    (i < 256 ? i + 6720 : i + 86235) // Event
    */

    auto landscape = co_await loading_scene->AddEntity<FFXILandscapeEntity>(105);
    //auto landscape = co_await loading_scene->AddEntity<FFXILandscapeEntity>(291, loading_scene.get());
    //audio->setMusic(79, 0);
    audio->setMusic(114, 0);
    //iroha 3111 (arciela 3074)
    //auto player = co_await loading_scene->AddEntity<Actor>(3111);
    auto [player, player_components] = co_await loading_scene->AddEntity<Actor>(FFXI::ActorPCModelsComponent::LookData{ .look = {
        .race = 2,
        .face = 15,
        .head = 0x1000 + 64,
        .body = 0x2000 + 64,
        .hands = 0x3000 + 64,
        .legs = 0x4000 + 64,
        .feet = 0x5000 + 64,
        .weapon = 0x6000 + 140,
        .weapon_sub = 0x7000 + 140
        }
    });
    //player->setPos(glm::vec3(-430.f, -42.2f, 46.f));
    //player->setPos(glm::vec3(259.f, -87.f, 99.f));
    auto camera = co_await loading_scene->AddEntity<ThirdPersonFFXIVCamera>(std::weak_ptr<lotus::Entity>(player));
    if (engine->config->renderer.render_mode == lotus::Config::Renderer::RenderMode::Rasterization)
    {
        //co_await camera->addComponent<lotus::CameraCascadesComponent>();
    }
    //co_await player->addComponent<ThirdPersonEntityFFXIVInputComponent>(engine->input.get());
    //co_await player->addComponent<ParticleTester>(engine->input.get());
    //co_await player->addComponent<EquipmentTestComponent>(engine->input.get());
    auto a = std::get<lotus::Component::AnimationComponent*>(player_components);
    auto ac = std::get<FFXI::ActorComponent*>(player_components);

    auto ac2 = loading_scene->component_runners->addComponent<FFXI::ModernThirdPersonInputComponent>(player.get(), *ac, *a);

    auto cam_c = co_await loading_scene->component_runners->addComponent<lotus::Component::CameraComponent>(camera.first.get());
    auto dur = loading_scene->component_runners->addComponent<FFXI::CameraThirdPersonComponent>(camera.first.get(), *cam_c, *ac);

    engine->set_camera(cam_c);
    engine->camera->setPerspective(glm::radians(70.f), engine->renderer->swapchain->extent.width / (float)engine->renderer->swapchain->extent.height, 0.01f, 1000.f);

    co_await update_scene(std::move(loading_scene));
}
