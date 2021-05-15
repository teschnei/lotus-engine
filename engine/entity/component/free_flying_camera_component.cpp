#include "free_flying_camera_component.h"
#include "engine/entity/camera.h"
#include "engine/input.h"
#include <iostream>

namespace lotus
{
    FreeFlyingCameraComponent::FreeFlyingCameraComponent(Entity* _entity, Engine* _engine, Input* _input) : InputComponent(_entity, _engine, _input)
    {
    }

    bool FreeFlyingCameraComponent::handleInput(const SDL_Event& event)
    {
        auto camera = static_cast<Camera*>(entity);
        if (event.type == SDL_MOUSEMOTION)
        {
            if (look)
            {
                static float speed = 0.005f;
                camera->look(event.motion.yrel * speed, -event.motion.xrel * speed);
                return true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                auto window = input->GetWindow();
                SDL_ShowCursor(SDL_FALSE);
                SDL_SetWindowGrab(window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                look = true;
                look_x = event.button.x;
                look_y = event.button.y;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP)
        {
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                auto window = input->GetWindow();
                SDL_ShowCursor(SDL_TRUE);
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                look = false;
                SDL_WarpMouseInWindow(window, look_x, look_y);
            }
        }
        else if (event.type == SDL_KEYDOWN)
        {
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_W:
                moving.x = 1;
                return true;
            case SDL_SCANCODE_S:
                moving.x = -1;
                return true;
            case SDL_SCANCODE_A:
                moving.z = -1;
                return true;
            case SDL_SCANCODE_D:
                moving.z = 1;
                return true;
            default:
                break;
            }
        }
        else if (event.type == SDL_KEYUP)
        {
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_W:
                moving.x = 0;
                return true;
            case SDL_SCANCODE_S:
                moving.x = 0;
                return true;
            case SDL_SCANCODE_A:
                moving.z = 0;
                return true;
            case SDL_SCANCODE_D:
                moving.z = 0;
                return true;
            default:
                break;
            }
        }
        return false;
    }

    Task<> FreeFlyingCameraComponent::tick(time_point time, duration delta)
    {
        auto camera = static_cast<Camera*>(entity);
        if (moving.x != 0 || moving.z != 0)
        {
            auto norm = glm::normalize(moving);
            static float speed = .000005f;
            auto ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
            camera->move(norm.x * ms * speed, norm.z * ms * speed);
            auto pos = camera->getPos();
            std::cout << "x: " << pos[0] << " y: " << pos[1] << " z: " << pos[2] << std::endl;
        }
        co_return;
    }
}
