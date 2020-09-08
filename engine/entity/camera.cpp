#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <algorithm>
#include "engine/core.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"

namespace lotus
{
    Camera::Camera(Engine* engine) : Entity(engine)
    {
        
    }

    Camera::~Camera()
    {
    }

    std::vector<UniqueWork> Camera::Init(const std::shared_ptr<Camera>& sp)
    {
        camera_rot.x = cos(rot_x) * cos(rot_y);
        camera_rot.y = sin(rot_x);
        camera_rot.z = cos(rot_x) * sin(rot_y);
        camera_rot = glm::normalize(camera_rot);

        update = true;
        return {};
    }

    void Camera::setPerspective(float radians, float aspect, float _near_clip, float _far_clip)
    {
        near_clip = _near_clip;
        far_clip = _far_clip;
        camera_data.proj = glm::perspective(radians, aspect, near_clip, far_clip);
        camera_data.proj[1][1] *= -1;
        camera_data.proj_inverse = glm::inverse(camera_data.proj);

        auto tangent = glm::tan(radians * 0.5);
        nh = near_clip * tangent;
        nw = nh * aspect;
        fh = far_clip * tangent;
        fw = fh * aspect;

        update = true;
    }

    void Camera::setPos(glm::vec3 pos)
    {
        Entity::setPos(pos);
        camera_data.view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        camera_data.view_inverse = glm::inverse(camera_data.view);
        camera_data.eye_pos = glm::vec4(pos, 0.0);
        update = true;
    }

    void Camera::move(float forward_offset, float right_offset)
    {
        pos += forward_offset * camera_rot;
        pos += right_offset * glm::normalize(glm::cross(camera_rot, glm::vec3(0.f, -1.f, 0.f)));
        camera_data.view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        camera_data.view_inverse = glm::inverse(camera_data.view);
        camera_data.eye_pos = glm::vec4(pos, 0.0);
        update = true;
    }

    void Camera::look(float rot_x_offset, float rot_y_offset)
    {
        rot_y += rot_y_offset;
        glm::mod(rot_y, glm::pi<float>());
        rot_x = std::clamp(rot_x += rot_x_offset, -glm::pi<float>() / 2, glm::pi<float>() / 2);

        camera_rot.x = cos(rot_x) * cos(rot_y);
        camera_rot.z = cos(rot_x) * sin(rot_y);
        camera_rot.y = sin(rot_x);
        camera_rot = glm::normalize(camera_rot);

        camera_data.view = glm::lookAt(pos, pos + camera_rot, glm::vec3(0.f, -1.f, 0.f));
        camera_data.view_inverse = glm::inverse(camera_data.view);
        update = true;
    }

    void Camera::updateBuffers(uint8_t* view_proj_mapped)
    {
        memcpy(view_proj_mapped + (engine->renderer->getCurrentImage() * engine->renderer->uniform_buffer_align_up(sizeof(CameraData))), &camera_data, sizeof(camera_data));
    }

    void Camera::tick(time_point time, duration delta)
    {
        if (update)
        {
            //The Gribb-Hartmann method seems a lot easier, but it doesn't seem to work with my vp matrix - will just use the longer way for now
            //glm::mat4 view_proj = view * proj;

            //frustum.left = glm::column(view_proj, 3) + glm::column(view_proj, 0);
            //frustum.right = glm::column(view_proj, 3) - glm::column(view_proj, 0);
            //frustum.top = glm::column(view_proj, 3) - glm::column(view_proj, 1);
            //frustum.bottom = glm::column(view_proj, 3) + glm::column(view_proj, 1);
            //frustum.near = glm::column(view_proj, 2);
            //frustum.far = glm::column(view_proj, 3) - glm::column(view_proj, 2);

            //auto length = glm::length(glm::vec3(frustum.left));
            //frustum.left.x /= length; frustum.left.y /= length; frustum.left.z /= length; frustum.left.w /= length;
            //length = glm::length(glm::vec3(frustum.right));
            //frustum.right.x /= length; frustum.right.y /= length; frustum.right.z /= length; frustum.right.w /= length;
            //length = glm::length(glm::vec3(frustum.top));
            //frustum.top.x /= length; frustum.top.y /= length; frustum.top.z /= length; frustum.top.w /= length;
            //length = glm::length(glm::vec3(frustum.bottom));
            //frustum.bottom.x /= length; frustum.bottom.y /= length; frustum.bottom.z /= length; frustum.bottom.w /= length;
            //length = glm::length(glm::vec3(frustum.near));
            //frustum.near.x /= length; frustum.near.y /= length; frustum.near.z /= length; frustum.near.w /= length;
            //length = glm::length(glm::vec3(frustum.far));
            //frustum.far.x /= length; frustum.far.y /= length; frustum.far.z /= length; frustum.far.w /= length;

            auto Z = -camera_rot;
            auto X = glm::normalize(glm::cross(glm::vec3{ 0.f, -1.f, 0.f }, Z));

            auto Y = glm::cross(Z, X);

            auto nc = pos - Z * near_clip;
            auto fc = pos - Z * far_clip;

            auto normal = glm::normalize(-Z);
            auto point = -glm::dot(normal, nc);
            frustum.near.x = normal.x; frustum.near.y = normal.y; frustum.near.z = normal.z; frustum.near.w = point;
            normal = glm::normalize(Z);
            point = -glm::dot(normal, fc);
            frustum.far.x = normal.x; frustum.far.y = normal.y; frustum.far.z = normal.z; frustum.far.w = point;

            auto aux = glm::normalize((nc + Y * nh) - pos);
            normal = glm::normalize(glm::cross(aux, X));
            point = -glm::dot(normal, nc + Y * nh);
            frustum.top.x = normal.x; frustum.top.y = normal.y; frustum.top.z = normal.z; frustum.top.w = point;
            aux = glm::normalize((nc - Y * nh) - pos);
            normal = glm::normalize(glm::cross(X, aux));
            point = -glm::dot(normal, nc - Y * nh);
            frustum.bottom.x = normal.x; frustum.bottom.y = normal.y; frustum.bottom.z = normal.z; frustum.bottom.w = point;
            aux = glm::normalize((nc - X * nw) - pos);
            normal = glm::normalize(glm::cross(aux, Y));
            point = -glm::dot(normal, nc - X * nw);
            frustum.left.x = normal.x; frustum.left.y = normal.y; frustum.left.z = normal.z; frustum.left.w = point;
            aux = glm::normalize((nc + X * nw) - pos);
            normal = glm::normalize(glm::cross(Y, aux));
            point = -glm::dot(normal, nc + X * nw);
            frustum.right.x = normal.x; frustum.right.y = normal.y; frustum.right.z = normal.z; frustum.right.w = point;
        }
    }

    void Camera::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        //components run first, which will do stuff on updates
        update = false;
    }
}
