#include "third_person_ffxiv_camera.h"

#include "component/third_person_ffxiv_camera_component.h"
#include "engine/core.h"

ThirdPersonFFXIVCamera::ThirdPersonFFXIVCamera(lotus::Engine* engine, std::weak_ptr<Entity>& focus) : lotus::ThirdPersonBoomCamera(engine, focus)
{
}

lotus::Task<std::shared_ptr<ThirdPersonFFXIVCamera>> ThirdPersonFFXIVCamera::Init(lotus::Engine* engine, std::weak_ptr<Entity>& focus)
{
    auto sp = std::make_shared<ThirdPersonFFXIVCamera>(engine, focus);
    sp->addComponent<ThirdPersonFFXIVCameraComponent>(engine->input.get(), focus);
    co_return sp;
}
