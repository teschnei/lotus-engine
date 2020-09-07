#include "third_person_ffxiv_camera.h"

#include "component/third_person_ffxiv_camera_component.h"
#include "engine/core.h"

ThirdPersonFFXIVCamera::ThirdPersonFFXIVCamera(lotus::Engine* engine) : lotus::ThirdPersonBoomCamera(engine)
{
    
}

std::vector<std::unique_ptr<lotus::WorkItem>> ThirdPersonFFXIVCamera::Init(const std::shared_ptr<ThirdPersonFFXIVCamera>& sp, std::weak_ptr<Entity>& _focus)
{
    focus = _focus;
    auto work = Camera::Init(sp);
    lotus::Input* input = engine->input.get();
    addComponent<ThirdPersonFFXIVCameraComponent>(input, focus);

    glm::quat pitch = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
    glm::quat yaw = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
    rot = glm::normalize(yaw * pitch);
    update = true;
    return work;
}
