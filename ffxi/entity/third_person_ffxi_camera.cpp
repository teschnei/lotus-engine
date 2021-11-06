#include "third_person_ffxi_camera.h"

//#include "component/third_person_ffxi_camera_component.h"
#include "engine/core.h"

lotus::Task<std::shared_ptr<lotus::Entity>> ThirdPersonFFXICamera::Init(lotus::Engine* engine, std::weak_ptr<lotus::Entity>& focus)
{
    auto sp = std::make_shared<lotus::Entity>();
    //co_await sp->addComponent<ThirdPersonFFXICameraComponent>(engine->input.get(), focus);
    co_return sp;
}
