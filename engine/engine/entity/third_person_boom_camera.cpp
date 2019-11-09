#include "third_person_boom_camera.h"

#include "component/third_person_camera_component.h"
#include "engine/core.h"

namespace lotus
{
    ThirdPersonBoomCamera::ThirdPersonBoomCamera() : Camera()
    {
        
    }

    void ThirdPersonBoomCamera::Init(const std::shared_ptr<ThirdPersonBoomCamera>& sp, Engine* engine, std::weak_ptr<Entity>& _focus)
    {
        focus = _focus;
        Camera::Init(sp, engine);
        Input* input = &engine->input;
        addComponent<ThirdPersonCameraComponent>(input, focus);

        glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
        glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
        rot = glm::normalize(pitch * yaw);
        updatePos();
    }

    void ThirdPersonBoomCamera::setDistance(float _distance)
    {
        distance = _distance;
        updatePos();
    }

    void ThirdPersonBoomCamera::look(glm::vec3 eye_focus)
    {
        updatePos();
        view = glm::lookAt(pos, eye_focus, glm::vec3(0.f, -1.f, 0.f));
        view_inverse = glm::inverse(view);
        update_ubo = true;
    }

    void ThirdPersonBoomCamera::swivel(float x_offset, float y_offset)
    {
        rot_x += x_offset;
        rot_y += y_offset;

        rot_y = std::clamp(rot_y, -((glm::pi<float>() / 2) - 0.01f), (glm::pi<float>() / 2) - 0.01f);
        if (rot_x > glm::pi<float>())
            rot_x -= glm::pi<float>() * 2;
        else if (rot_x < -glm::pi<float>())
            rot_x += glm::pi<float>() * 2;

        glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
        glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
        rot = glm::normalize(pitch * yaw);
        updatePos();
    }

    void ThirdPersonBoomCamera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        update_ubo = true;
    }

    void ThirdPersonBoomCamera::updatePos()
    {
        glm::vec3 boom{ distance, 0.f, 0.f };
        glm::vec3 new_pos = boom * rot;
        Entity::setPos(new_pos + focus.lock()->getPos());
        update_ubo = true;
    }
}
