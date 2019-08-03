#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "component/camera_component.h"

namespace lotus
{
    Camera::Camera(Input* input)
    {
        camera_rot.x = cos(rot_x) * cos(rot_z);
        camera_rot.y = sin(rot_x);
        camera_rot.z = cos(rot_x) * sin(rot_z);
        camera_rot = glm::normalize(camera_rot);
        addComponent<CameraComponent>(input);
    }

    void Camera::setPerspective(float radians, float aspect, float near, float far)
    {
        proj = glm::perspective(radians, aspect, near, far);
        proj[1][1] *= -1;
    }

    void Camera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
    }

    void Camera::move(float forward_offset, float right_offset)
    {
        pos += forward_offset * camera_rot;
        pos += right_offset * glm::normalize(glm::cross(camera_rot, glm::vec3(0.f, -1.f, 0.f)));
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
    }

    void Camera::look(float rot_x_offset, float rot_z_offset)
    {
        rot_z += rot_z_offset;
        glm::mod(rot_z, glm::pi<float>());
        rot_x = std::clamp(rot_x += rot_x_offset, -glm::pi<float>() / 2, glm::pi<float>() / 2);

        camera_rot.x = cos(rot_x) * cos(rot_z);
        camera_rot.z = cos(rot_x) * sin(rot_z);
        camera_rot.y = sin(rot_x);
        camera_rot = glm::normalize(camera_rot);

        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
    }
}
