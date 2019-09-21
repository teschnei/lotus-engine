#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "entity.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/renderer.h"

namespace lotus
{
    class Engine;
    class Input;

    class Camera : public Entity
    {
    public:
        Camera(Engine* engine, Input* input);
        glm::mat4& getViewMatrix() { return view; }
        glm::mat4& getProjMatrix() { return proj; }

        void setPos(glm::vec3);
        void setPerspective(float radians, float aspect, float near_clip, float far_clip);
        float getNearClip() { return near_clip; }
        float getFarClip() { return far_clip; }
        void move(float forward_offset, float right_offset);
        void look(float rot_x_offset, float rot_y_offset);

        glm::vec3 getRotationVector() { return camera_rot; }

        std::unique_ptr<Buffer> view_proj_ubo;

        struct UBOFS
        {
            glm::vec4 cascade_splits;
            glm::mat4 cascade_view_proj[Renderer::shadowmap_cascades];
            glm::mat4 inverse_view;
        } cascade_data;

        std::unique_ptr<Buffer> cascade_data_ubo;


    private:
        virtual void tick(time_point time, duration delta) override;

        float rot_x{0};
        float rot_z{ -glm::pi<float>()  };
        float near_clip{ 0.f };
        float far_clip{ 0.f };
        glm::vec3 camera_rot{};

        glm::mat4 view{};
        glm::mat4 proj{};
        glm::mat4 view_inverse{};
        glm::mat4 proj_inverse{};

        bool update_ubo{ false };
        Engine* engine{ nullptr };
    };
}
