#pragma once
#include "component.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/renderer/memory.h"

namespace lotus::Test
{
    class CameraComponent : public Component<CameraComponent>
    {
    public:
        explicit CameraComponent(Entity*, Engine* engine);

        Task<> tick(time_point time, duration delta);
        Task<> init();

        void setPos(glm::vec3 pos);
        void setTarget(glm::vec3 target);
        void setPerspective(float fov, float aspect_ratio, float near_clip, float far_clip);
        glm::mat4 getViewMatrix();

        struct CameraData
        {
            glm::mat4 proj{};
            glm::mat4 view{};
            glm::mat4 proj_inverse{};
            glm::mat4 view_inverse{};
            glm::vec4 eye_pos{};
        };

        void writeToBuffer(CameraData& buffer);

    protected:
        bool update_view{ true };
        glm::vec3 pos{ 0.f };
        glm::vec3 target{ 1.f };
        glm::mat4 view{};
        glm::mat4 view_inverse{};

        bool update_projection{ true };
        float fov{};
        float aspect_ratio{};
        float near_clip{ 0.f };
        float far_clip{ 0.f };
        glm::mat4 projection{};
        glm::mat4 projection_inverse{};
    };
}
