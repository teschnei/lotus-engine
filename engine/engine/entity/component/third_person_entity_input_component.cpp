#include "third_person_entity_input_component.h"
#include "engine/entity/camera.h"
#include "engine/input.h"

namespace lotus
{
    ThirdPersonEntityInputComponent::ThirdPersonEntityInputComponent(Entity* _entity, Input* _input) : InputComponent(_entity, _input)
    {
    }

    bool ThirdPersonEntityInputComponent::handleInput(const SDL_Event& event)
    {
        if (event.type == SDL_KEYDOWN)
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
                moving.z = 1;
                return true;
            case SDL_SCANCODE_D:
                moving.z = -1;
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

    void ThirdPersonEntityInputComponent::tick(time_point time, duration delta)
    {
        if (moving.x != 0 || moving.z != 0)
        {
            auto norm = glm::normalize(moving);
            static float speed = .000005f;
            auto ms = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
            float forward_offset = norm.x * ms * speed;
            float right_offset = norm.z * ms * speed;
            glm::vec3 offset{ forward_offset, 0.f, right_offset };
            auto pos = entity->getPos();
            auto rot = entity->getRot();
            pos += offset * rot;
            entity->setPos(pos);
        }
    }
}
