#pragma once
#include "engine/entity/component/input_component.h"
#include <memory>

namespace lotus
{
    class ThirdPersonCameraComponent : public InputComponent
    {
    public:
        explicit ThirdPersonCameraComponent(Entity*, Input*, std::weak_ptr<Entity>& focus);
        virtual bool handleInput(const SDL_Event&) override;
        virtual void tick(time_point time, duration delta) override;
    protected:
        std::weak_ptr<Entity> focus;
        //min/max camera distance (squared)
        float max_camera_distance{ 100.f };
        float min_camera_distance{ 10.f };
        enum class Look
        {
            NoLook,
            LookCamera,
            LookBoth
        };
        Look look{ Look::NoLook };
        //x/y of mouse when mouselook started
        int look_x{ 0 };
        int look_y{ 0 };
    };
}
