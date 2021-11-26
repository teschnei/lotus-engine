#include "third_person_ffxiv_camera.h"

#include "engine/core.h"
#include "engine/scene.h"
#include "component/camera_third_person_component.h"
#include "engine/entity/component/camera_cascades_component.h"

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<lotus::Component::CameraComponent*>>> ThirdPersonFFXIVCamera::Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorComponent* actor_component)
{
    auto sp = std::make_shared<lotus::Entity>();
    auto cam_c = co_await lotus::Component::CameraComponent::make_component(sp.get(), engine);
    auto dur = co_await FFXI::CameraThirdPersonComponent::make_component(sp.get(), engine, *cam_c, *actor_component);
    auto cc = engine->config->renderer.render_mode == lotus::Config::Renderer::RenderMode::Rasterization ? co_await lotus::Component::CameraCascadesComponent::make_component(sp.get(), engine, *cam_c) : nullptr;
    auto p = scene->AddComponents(std::move(cam_c), std::move(dur), std::move(cc));
    co_return std::make_pair(sp, std::get<lotus::Component::CameraComponent*>(p));
}
