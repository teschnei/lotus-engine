#include "third_person_ffxi_camera.h"

#include "component/third_person_ffxi_camera_component.h"
#include "engine/core.h"
#include "engine/work_item.h"

ThirdPersonFFXICamera::ThirdPersonFFXICamera(lotus::Engine* engine) : lotus::ThirdPersonBoomCamera(engine)
{
    
}

std::vector<std::unique_ptr<lotus::WorkItem>> ThirdPersonFFXICamera::Init(const std::shared_ptr<ThirdPersonFFXICamera>& sp, std::weak_ptr<Entity>& _focus)
{
    focus = _focus;
    auto work = Camera::Init(sp);
    lotus::Input* input = engine->input.get();
    addComponent<ThirdPersonFFXICameraComponent>(input, focus);

    glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
    glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
    rot = glm::normalize(pitch * yaw);
    glm::vec3 boom_source = focus.lock()->getPos() + glm::vec3{0.f, -0.5f, 0.f};
    glm::vec3 boom{ distance, 0.f, 0.f };
    glm::vec3 new_pos = boom * rot;
    Camera::setPos(new_pos + boom_source);
    update = true;
    return work;
}
