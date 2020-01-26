#include "third_person_ffxiv_camera_component.h"
#include "engine/entity/camera.h"
#include "engine/input.h"
#include <iostream>
#include "entity/third_person_ffxiv_camera.h"
#include <glm/gtx/vector_angle.hpp>

ThirdPersonFFXIVCameraComponent::ThirdPersonFFXIVCameraComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input, std::weak_ptr<lotus::Entity>& _focus) : InputComponent(_entity, _engine, _input), focus(_focus)
{
}

bool ThirdPersonFFXIVCameraComponent::handleInput(const SDL_Event& event)
{
    auto camera = static_cast<ThirdPersonFFXIVCamera*>(entity);
    if (event.type == SDL_MOUSEMOTION)
    {
        if (look == Look::LookCamera || look == Look::LookBoth)
        {
            static float speed = 0.005f;
            camera->swivel(-event.motion.xrel * speed, -event.motion.yrel * speed);
            if (look == Look::LookBoth)
            {
                auto focus_sp = focus.lock();
                if (focus_sp)
                {
                    auto focus_pos = focus_sp->getPos();
                    auto camera_pos = entity->getPos();
                    glm::vec2 dir = glm::normalize(glm::vec2{focus_pos.x, focus_pos.z} - glm::vec2{camera_pos.x, camera_pos.z});
                    auto angle = glm::orientedAngle(dir, glm::vec2(1.f, 0.f));
                    focus_sp->setRot(glm::angleAxis(-angle, glm::vec3{ 0.f, 1.f, 0.f }));
                }
            }
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
            look = event.button.button == SDL_BUTTON_LEFT ? Look::LookCamera : Look::LookBoth;
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
    return false;
}

void ThirdPersonFFXIVCameraComponent::tick(lotus::time_point time, lotus::duration delta)
{
    auto camera = static_cast<ThirdPersonFFXIVCamera*>(entity);
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
}
