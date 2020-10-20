#include "third_person_ffxi_camera.h"

#include "component/third_person_ffxi_camera_component.h"
#include "engine/core.h"
#include "engine/work_item.h"

ThirdPersonFFXICamera::ThirdPersonFFXICamera(lotus::Engine* engine, std::weak_ptr<Entity>& _focus) : lotus::ThirdPersonBoomCamera(engine, _focus)
{
}

lotus::Task<std::shared_ptr<ThirdPersonFFXICamera>> ThirdPersonFFXICamera::Init(lotus::Engine* engine, std::weak_ptr<Entity>& focus)
{
    auto sp = std::make_shared<ThirdPersonFFXICamera>(engine, focus);
    sp->addComponent<ThirdPersonFFXICameraComponent>(engine->input.get(), focus);
    co_return sp;
}
