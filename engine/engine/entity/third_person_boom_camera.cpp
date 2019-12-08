#include "third_person_boom_camera.h"

#include "component/third_person_camera_component.h"
#include "engine/core.h"
#include <iostream>

namespace lotus
{
    ThirdPersonBoomCamera::ThirdPersonBoomCamera(Engine* engine) : Camera(engine)
    {
        
    }

    void ThirdPersonBoomCamera::Init(const std::shared_ptr<ThirdPersonBoomCamera>& sp, std::weak_ptr<Entity>& _focus)
    {
        focus = _focus;
        Camera::Init(sp);
        Input* input = &engine->input;
        addComponent<ThirdPersonCameraComponent>(input, focus);

        glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
        glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
        rot = glm::normalize(pitch * yaw);
        update_pos = true;
    }

    void ThirdPersonBoomCamera::setDistance(float _distance)
    {
        distance = _distance;
        update_pos = true;
    }

    void ThirdPersonBoomCamera::look(glm::vec3 eye_focus)
    {
        update_pos = true;
        //TODO: base this value off a portion of height/bounding box
        glm::vec3 eye_focus_midpoint = eye_focus + glm::vec3{ 0.f, -0.5f, 0.f };
        view = glm::lookAt(pos, eye_focus_midpoint, glm::vec3(0.f, -1.f, 0.f));
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
        update_pos = true;
    }

    void ThirdPersonBoomCamera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        update_ubo = true;
    }

    void ThirdPersonBoomCamera::tick(time_point time, duration delta)
    {
        if (update_pos)
        {
            glm::vec3 boom_source = focus.lock()->getPos() + glm::vec3{0.f, -0.5f, 0.f};
            engine->renderer.raytracer->query(Raytracer::ObjectFlags::LevelGeometry, boom_source, glm::vec3{ 1.f, 0.f, 0.f } * rot, 0.f, distance, [this, boom_source](float new_distance)
            {
                glm::vec3 boom{ new_distance - 0.05f, 0.f, 0.f };
                glm::vec3 new_pos = boom * rot;
                Entity::setPos(new_pos + boom_source);
                update_ubo = true;
            });
        }
    }
}
