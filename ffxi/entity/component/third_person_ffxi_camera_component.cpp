#include "third_person_ffxi_camera_component.h"
#include "engine/entity/camera.h"
#include "engine/input.h"
#include <iostream>
#include "entity/third_person_ffxi_camera.h"
#include <glm/gtx/vector_angle.hpp>

ThirdPersonFFXICameraComponent::ThirdPersonFFXICameraComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input, std::weak_ptr<lotus::Entity>& _focus) : InputComponent(_entity, _engine, _input), focus(_focus)
{
}

bool ThirdPersonFFXICameraComponent::handleInput(const SDL_Event& event)
{
    auto camera = static_cast<ThirdPersonFFXICamera*>(entity);
    if (event.type == SDL_MOUSEMOTION)
    {
        if (look == Look::LookCamera)
        {
            static float speed = 0.005f;
            camera->swivel(-event.motion.xrel * speed, event.motion.yrel * speed);
            return true;
        }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN)
    {
        if (event.button.button == SDL_BUTTON_RIGHT ||
            event.button.button == SDL_BUTTON_LEFT)
        {
            auto window = input->GetWindow();
            SDL_ShowCursor(SDL_FALSE);
            SDL_SetWindowGrab(window, SDL_TRUE);
            SDL_SetRelativeMouseMode(SDL_TRUE);
            look = Look::LookCamera;
            look_x = event.button.x;
            look_y = event.button.y;
        }
    }
    else if (event.type == SDL_MOUSEBUTTONUP)
    {
        if (event.button.button == SDL_BUTTON_RIGHT ||
            event.button.button == SDL_BUTTON_LEFT)
        {
            auto window = input->GetWindow();
            SDL_ShowCursor(SDL_TRUE);
            SDL_SetWindowGrab(window, SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            look = Look::NoLook;
            SDL_WarpMouseInWindow(window, look_x, look_y);
        }
    }
    else if (event.type == SDL_MOUSEWHEEL)
    {
        if (event.wheel.y != 0)
        {
            camera->setDistance(std::clamp(camera->getDistance() - event.wheel.y, 1.f, 10.f));
        }
    }
    return false;
}

lotus::Task<> ThirdPersonFFXICameraComponent::tick(lotus::time_point time, lotus::duration delta)
{
    auto camera = static_cast<ThirdPersonFFXICamera*>(entity);
    auto focus_lock = focus.lock();
    if (focus_lock)
    {
        if (last_focus_pos != focus_lock->getPos())
        {
            camera->update_camera();
        }
    }
    else
    {
        //destroy self as focus is no longer valid
    }
    co_return;
}
