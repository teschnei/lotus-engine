#include "third_person_ffxiv_camera.h"

#include "engine/core.h"
#include "engine/scene.h"
#include "component/camera_third_person_component.h"

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<lotus::Component::CameraComponent*>>> ThirdPersonFFXIVCamera::Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorComponent* actor_component)
{
    auto sp = std::make_shared<lotus::Entity>();
    auto cam_c = co_await scene->component_runners->addComponent<lotus::Component::CameraComponent>(sp.get());
    auto dur = scene->component_runners->addComponent<FFXI::CameraThirdPersonComponent>(sp.get(), *cam_c, *actor_component);
    if (engine->config->renderer.render_mode == lotus::Config::Renderer::RenderMode::Rasterization)
    {
        //co_await camera->addComponent<lotus::CameraCascadesComponent>();
    }
    co_return std::make_pair(sp, cam_c);
}
