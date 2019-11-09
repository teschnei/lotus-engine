#include "third_person_ffxiv_camera.h"

#include "component/third_person_ffxiv_camera_component.h"
#include "engine/core.h"

ThirdPersonFFXIVCamera::ThirdPersonFFXIVCamera() : lotus::ThirdPersonBoomCamera()
{
    
}

void ThirdPersonFFXIVCamera::Init(const std::shared_ptr<ThirdPersonFFXIVCamera>& sp, lotus::Engine* engine, std::weak_ptr<Entity>& _focus)
{
    focus = _focus;
    Camera::Init(sp, engine);
    lotus::Input* input = &engine->input;
    addComponent<ThirdPersonFFXIVCameraComponent>(input, focus);

    glm::quat pitch = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
    glm::quat yaw = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
    rot = glm::normalize(yaw * pitch);
    updatePos();
}
