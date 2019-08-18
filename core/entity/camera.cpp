#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "component/camera_component.h"
#include "core.h"

namespace lotus
{
    Camera::Camera(Engine* _engine, Input* input) : engine(_engine)
    {
        camera_rot.x = cos(rot_x) * cos(rot_z);
        camera_rot.y = sin(rot_x);
        camera_rot.z = cos(rot_x) * sin(rot_z);
        camera_rot = glm::normalize(camera_rot);
        addComponent<CameraComponent>(input);

        view_proj_ubo = engine->renderer.memory_manager->GetBuffer((sizeof(view) + sizeof(proj)) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void Camera::setPerspective(float radians, float aspect, float _near_clip, float _far_clip)
    {
        near_clip = _near_clip;
        far_clip = _far_clip;
        proj = glm::perspective(radians, aspect, near_clip, far_clip);
        proj[1][1] *= -1;
        update_ubo = true;
    }

    void Camera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        update_ubo = true;
    }

    void Camera::move(float forward_offset, float right_offset)
    {
        pos += forward_offset * camera_rot;
        pos += right_offset * glm::normalize(glm::cross(camera_rot, glm::vec3(0.f, -1.f, 0.f)));
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        update_ubo = true;
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
        update_ubo = true;
    }

    void Camera::tick(time_point time, duration delta)
    {
        if (update_ubo)
        {
            engine->worker_pool.addWork(std::make_unique<LambdaWorkItem>([this](WorkerThread* thread)
            {
                void* buf = thread->engine->renderer.device->mapMemory(view_proj_ubo->memory, view_proj_ubo->memory_offset + (sizeof(view) + sizeof(proj)) * engine->renderer.getCurrentImage(), sizeof(view) + sizeof(proj), {}, thread->engine->renderer.dispatch);
                memcpy(buf, &proj, sizeof(proj));
                memcpy(static_cast<uint8_t*>(buf) + sizeof(proj), &view, sizeof(view));
                thread->engine->renderer.device->unmapMemory(view_proj_ubo->memory, thread->engine->renderer.dispatch);
            }));
        }
    }
}
