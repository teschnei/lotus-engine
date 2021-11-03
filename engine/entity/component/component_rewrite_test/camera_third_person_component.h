#pragma once
#include "component.h"
#include "camera_component.h"
#include "physics_component.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/renderer/memory.h"

namespace lotus
{
    class Input;
}

namespace lotus::Test
{
    class CameraThirdPersonComponent : public Component<CameraThirdPersonComponent, CameraComponent, PhysicsComponent>
    {
    public:
        explicit CameraThirdPersonComponent(Entity*, Engine* engine, CameraComponent& camera, PhysicsComponent& target);

        Task<> tick(time_point time, duration delta);
        bool handleInput(Input*, const SDL_Event&);

        void setDistance(float _distance);
        float getDistance() const { return distance; }
        void swivel(float x_offset, float y_offset);

    protected:
        //camera
        float rot_x{};
        float rot_y{};
        glm::quat rot{1.f, 0.f, 0.f, 0.f};
        float distance{ 7.f };

        //input
        enum class Look
        {
            NoLook,
            LookCamera,
            LookBoth
        };
        Look look{ Look::NoLook };
        glm::ivec2 look_pos{};
    };
}
