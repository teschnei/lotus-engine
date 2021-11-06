#include "third_person_ffxiv_camera.h"

//#include "component/third_person_ffxiv_camera_component.h"
#include "engine/core.h"

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> ThirdPersonFFXIVCamera::Init(lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity>& focus)
{
    auto sp = std::make_shared<lotus::Entity>();
    //co_await sp->addComponent<ThirdPersonFFXIVCameraComponent>(engine->input.get(), focus);
    co_return std::make_pair(sp, std::tuple<>());
}
