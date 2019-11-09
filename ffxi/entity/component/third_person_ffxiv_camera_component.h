#pragma once
#include "engine/entity/component/input_component.h"
#include <memory>

class ThirdPersonFFXIVCameraComponent : public lotus::InputComponent
{
public:
    explicit ThirdPersonFFXIVCameraComponent(lotus::Entity*, lotus::Input*, std::weak_ptr<lotus::Entity>& focus);
    virtual bool handleInput(const SDL_Event&) override;
    virtual void tick(lotus::time_point time, lotus::duration delta) override;
protected:
    std::weak_ptr<lotus::Entity> focus;
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
