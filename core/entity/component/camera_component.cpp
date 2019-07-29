#include "camera_component.h"
#include "../camera.h"

namespace lotus
{
    CameraComponent::CameraComponent(Entity* _entity, Input* _input) : InputComponent(_entity, _input)
    {
    }

    bool CameraComponent::handleInput(const SDL_Event& event)
    {
        auto camera = static_cast<Camera*>(entity);
        if (event.type == SDL_MOUSEMOTION)
        {
            static float speed = 0.005f;
            camera->look(event.motion.yrel * speed, -event.motion.xrel * speed);
            return true;
        }
        else if (event.type == SDL_KEYDOWN)
        {
            static float speed = 5.f;
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_W:
                camera->move(speed, 0);
                break;
            case SDL_SCANCODE_S:
                camera->move(-speed, 0);
                break;
            case SDL_SCANCODE_A:
                camera->move(0, -speed);
                break;
            case SDL_SCANCODE_D:
                camera->move(0, speed);
                break;
            default:
                break;
            }
            return true;
        }
        return false;
    }

    void CameraComponent::tick()
    {
    }
}
