#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "component/camera_component.h"
#include "engine/core.h"

namespace lotus
{
    Camera::Camera(Engine* _engine, Input* input) : engine(_engine)
    {
        camera_rot.x = cos(rot_x) * cos(rot_z);
        camera_rot.y = sin(rot_x);
        camera_rot.z = cos(rot_x) * sin(rot_z);
        camera_rot = glm::normalize(camera_rot);
        addComponent<CameraComponent>(input);

        view_proj_ubo = engine->renderer.memory_manager->GetBuffer((sizeof(view) + sizeof(proj)) * 2 * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        if (engine->renderer.render_mode == RenderMode::Rasterization)
        {
            cascade_data_ubo = engine->renderer.memory_manager->GetBuffer(sizeof(cascade_data) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }
    }

    void Camera::setPerspective(float radians, float aspect, float _near_clip, float _far_clip)
    {
        near_clip = _near_clip;
        far_clip = _far_clip;
        proj = glm::perspective(radians, aspect, near_clip, far_clip);
        proj[1][1] *= -1;
        proj_inverse = glm::inverse(proj);
        update_ubo = true;
    }

    void Camera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        view_inverse = glm::inverse(view);
        update_ubo = true;
    }

    void Camera::move(float forward_offset, float right_offset)
    {
        pos += forward_offset * camera_rot;
        pos += right_offset * glm::normalize(glm::cross(camera_rot, glm::vec3(0.f, -1.f, 0.f)));
        view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        view_inverse = glm::inverse(view);
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
        view_inverse = glm::inverse(view);
        update_ubo = true;
    }

    void Camera::tick(time_point time, duration delta)
    {
        if (update_ubo)
        {
            engine->worker_pool.addWork(std::make_unique<LambdaWorkItem>([this](WorkerThread* thread)
            {
                void* buf = thread->engine->renderer.device->mapMemory(view_proj_ubo->memory, view_proj_ubo->memory_offset + (sizeof(view) + sizeof(proj)) * 2 * engine->renderer.getCurrentImage(), (sizeof(view) + sizeof(proj)) * 2, {}, thread->engine->renderer.dispatch);
                memcpy(buf, &proj, sizeof(proj));
                memcpy(static_cast<uint8_t*>(buf) + sizeof(proj), &view, sizeof(view));
                memcpy(static_cast<uint8_t*>(buf) + sizeof(proj) * 2, &proj_inverse, sizeof(proj_inverse));
                memcpy(static_cast<uint8_t*>(buf) + sizeof(proj) * 3, &view_inverse, sizeof(view_inverse));
                thread->engine->renderer.device->unmapMemory(view_proj_ubo->memory, thread->engine->renderer.dispatch);

                if (thread->engine->renderer.render_mode == RenderMode::Rasterization)
                {
                    glm::vec3 lightDir = thread->engine->lights.directional_light.direction;
                    float cascade_splits[lotus::Renderer::shadowmap_cascades];

                    float near_clip = thread->engine->camera.getNearClip();
                    float far_clip = thread->engine->camera.getFarClip();
                    float range = far_clip - near_clip;
                    float ratio = far_clip / near_clip;

                    for (size_t i = 0; i < lotus::Renderer::shadowmap_cascades; ++i)
                    {
                        float p = (i + 1) / static_cast<float>(lotus::Renderer::shadowmap_cascades);
                        float log = near_clip * std::pow(ratio, p);
                        float uniform = near_clip + range * p;
                        float d = 0.95f * (log - uniform) + uniform;
                        cascade_splits[i] = (d - near_clip) / range;
                    }

                    float last_split = 0.0f;

                    for (uint32_t i = 0; i < lotus::Renderer::shadowmap_cascades; ++i)
                    {
                        float split_dist = cascade_splits[i];
                        std::array<glm::vec3, 8> frustum_corners = {
                            glm::vec3{-1.f, 1.f, -1.f},
                            glm::vec3{1.f, 1.f, -1.f},
                            glm::vec3{1.f, -1.f, -1.f},
                            glm::vec3{-1.f, -1.f, -1.f},
                            glm::vec3{-1.f, 1.f, 1.f},
                            glm::vec3{1.f, 1.f, 1.f},
                            glm::vec3{1.f, -1.f, 1.f},
                            glm::vec3{-1.f, -1.f, 1.f}
                        };

                        glm::mat4 inverse_camera = glm::inverse(thread->engine->camera.getProjMatrix() * thread->engine->camera.getViewMatrix());

                        for (auto& corner : frustum_corners)
                        {
                            glm::vec4 inverse_corner = inverse_camera * glm::vec4{ corner, 1.f };
                            corner = inverse_corner / inverse_corner.w;
                        }

                        for (size_t i = 0; i < 4; ++i)
                        {
                            glm::vec3 distance = frustum_corners[i + 4] - frustum_corners[i];
                            frustum_corners[i + 4] = frustum_corners[i] + (distance * split_dist);
                            frustum_corners[i] = frustum_corners[i] + (distance * last_split);
                        }

                        glm::vec3 center = glm::vec3{ 0.f };
                        for (auto& corner : frustum_corners)
                        {
                            center += corner;
                        }
                        center /= 8.f;

                        float radius = 0.f;

                        for (auto& corner : frustum_corners)
                        {
                            float distance = glm::length(corner - center);
                            radius = glm::max(radius, distance);
                        }
                        radius = std::ceil(radius * 16.f) / 16.f;

                        glm::vec3 max_extents = glm::vec3(radius);
                        glm::vec3 min_extents = -max_extents;

                        glm::mat4 light_view = glm::lookAt(center - lightDir * -min_extents.z, center, glm::vec3{ 0.f, -1.f, 0.f });
                        glm::mat4 light_ortho = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, min_extents.z * 2, max_extents.z * 2);
                        light_ortho[1][1] *= -1;

                        cascade_data.cascade_splits[i] =  (near_clip + split_dist * range) * -1.f;
                        cascade_data.cascade_view_proj[i] = light_ortho * light_view;

                        last_split = cascade_splits[i];
                    }
                    cascade_data.inverse_view = glm::inverse(thread->engine->camera.getViewMatrix());
                    auto data = thread->engine->renderer.device->mapMemory(cascade_data_ubo->memory, cascade_data_ubo->memory_offset, sizeof(cascade_data) * engine->renderer.getImageCount(), {}, thread->engine->renderer.dispatch);
                    memcpy(static_cast<uint8_t*>(data) + (thread->engine->renderer.getCurrentImage() * sizeof(cascade_data)), &cascade_data, sizeof(cascade_data));
                    thread->engine->renderer.device->unmapMemory(cascade_data_ubo->memory, thread->engine->renderer.dispatch);
                }
            }));
        }
    }
}
