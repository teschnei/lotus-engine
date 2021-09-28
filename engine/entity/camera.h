#pragma once

#include "entity.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/vulkan/renderer.h"
#include <glm/glm.hpp>

//i love windows
#undef near
#undef far

namespace lotus
{
    class Engine;
    class Input;

    class Camera : public Entity
    {
    public:
        explicit Camera(Engine*);
        ~Camera();
        static Task<std::shared_ptr<Camera>> Init(Engine*);
        glm::mat4& getViewMatrix() { return camera_data.view; }
        glm::mat4& getProjMatrix() { return camera_data.proj; }

        void setPos(glm::vec3);
        void setPerspective(float radians, float aspect, float near_clip, float far_clip);
        float getNearClip() { return near_clip; }
        float getFarClip() { return far_clip; }
        void move(float forward_offset, float right_offset);
        void look(float rot_x_offset, float rot_y_offset);
        float getRotX() const { return rot_x; }
        float getRotY() const { return rot_y; }

        bool updated() { return update; }

        glm::vec3 getRotationVector() { return camera_rot; }

        void updateBuffers(uint8_t* view_proj);

        struct CameraData
        {
            glm::mat4 proj{};
            glm::mat4 view{};
            glm::mat4 proj_inverse{};
            glm::mat4 view_inverse{};
            glm::mat4 proj_prev{};
            glm::mat4 view_prev{};
            glm::vec4 eye_pos{};
        } camera_data;

        struct Frustum
        {
            glm::vec4 left;
            glm::vec4 right;
            glm::vec4 top;
            glm::vec4 bottom;
            glm::vec4 near;
            glm::vec4 far;
        } frustum {};

    protected:
        virtual Task<> tick(time_point time, duration delta) override;
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;

        float rot_x{ -glm::pi<float>() };
        float rot_y{ 0.f };
        float near_clip{ 0.f };
        float far_clip{ 0.f };
        glm::vec3 camera_rot{};
        glm::mat4 view_prev_temp{};

        float nh{};
        float nw{};
        float fh{};
        float fw{};

        bool update{ false };
    };
}
