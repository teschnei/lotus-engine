#include "third_person_boom_camera.h"

#include "component/third_person_camera_component.h"
#include "engine/core.h"
#include <iostream>

namespace lotus
{
    ThirdPersonBoomCamera::ThirdPersonBoomCamera(Engine* engine, std::weak_ptr<Entity>& _focus) : Camera(engine), focus(_focus)
    {
        glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
        glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
        rot = glm::normalize(pitch * yaw);
        glm::vec3 boom_source = focus.lock()->getPos() + glm::vec3{0.f, -0.5f, 0.f};
        glm::vec3 boom{ distance, 0.f, 0.f };
        glm::vec3 new_pos = boom * rot;
        Camera::setPos(new_pos + boom_source);
        look(boom_source);
        update = true;
    }

    Task<std::shared_ptr<ThirdPersonBoomCamera>> ThirdPersonBoomCamera::Init(Engine* engine, std::weak_ptr<Entity>& focus)
    {
        auto sp = std::make_shared<ThirdPersonBoomCamera>(engine, focus);
        co_await sp->addComponent<ThirdPersonCameraComponent>(engine->input.get(), focus);
        co_return sp;
    }

    void ThirdPersonBoomCamera::setDistance(float _distance)
    {
        distance = _distance;
        update = true;
    }

    void ThirdPersonBoomCamera::look(glm::vec3 eye_focus)
    {
        //TODO: base this value off a portion of height/bounding box
        glm::vec3 eye_focus_midpoint = eye_focus + glm::vec3{ 0.f, -0.5f, 0.f };
        camera_data.view = glm::lookAt(pos, eye_focus_midpoint, glm::vec3(0.f, 1.f, 0.f));
        camera_rot = glm::normalize(eye_focus_midpoint - pos);
        camera_data.view_inverse = glm::inverse(camera_data.view);
        update = true;
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
        update = true;
    }

    void ThirdPersonBoomCamera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        camera_data.eye_pos = glm::vec4(pos, 0);
        update = true;
    }

    Task<> ThirdPersonBoomCamera::tick(time_point time, duration delta)
    {
        if (update)
        {
            glm::vec3 boom_source = focus.lock()->getPos() + glm::vec3{0.f, -0.5f, 0.f};
            auto new_distance = co_await engine->renderer->raytrace_queryer->query(RaytraceQueryer::ObjectFlags::LevelCollision, boom_source, glm::vec3{ 1.f, 0.f, 0.f } *rot, 0.f, distance);
            glm::vec3 boom{ new_distance - 0.05f, 0.f, 0.f };
            glm::vec3 new_pos = boom * rot;
            if (new_pos + boom_source != getPos())
            {
                Entity::setPos(new_pos + boom_source);
                camera_data.eye_pos = glm::vec4(pos, 0);
                look(boom_source);
            }
        }
        co_await Camera::tick(time, delta);
    }
}
