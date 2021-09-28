#include "third_person_entity_input_component.h"
#include "engine/core.h"
#include "engine/entity/camera.h"
#include "engine/input.h"
#include "engine/renderer/raytrace_query.h"

namespace lotus
{
    ThirdPersonEntityInputComponent::ThirdPersonEntityInputComponent(Entity* _entity, Engine* _engine, Input* _input) : InputComponent(_entity, _engine, _input)
    {
    }

    bool ThirdPersonEntityInputComponent::handleInput(const SDL_Event& event)
    {
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
        {
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_W:
                if (moving.x == 0)
                    moving.x = 1;
                else moving.x = 0;
                return true;
            case SDL_SCANCODE_S:
                if (moving.x == 0)
                    moving.x = -1;
                else moving.x = 0;
                return true;
            case SDL_SCANCODE_A:
                if (moving.z == 0)
                    moving.z = 1;
                else moving.z = 0;
                return true;
            case SDL_SCANCODE_D:
                if (moving.z == 0)
                    moving.z = -1;
                else moving.z = 0;
                return true;
            default:
                break;
            }
        }
        else if (event.type == SDL_KEYUP && event.key.repeat == 0)
        {
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_W:
                if (moving.x == 0)
                    moving.x = -1;
                else moving.x = 0;
                return true;
            case SDL_SCANCODE_S:
                if (moving.x == 0)
                    moving.x = 1;
                else moving.x = 0;
                return true;
            case SDL_SCANCODE_A:
                if (moving.z == 0)
                    moving.z = -1;
                else moving.z = 0;
                return true;
            case SDL_SCANCODE_D:
                if (moving.z == 0)
                    moving.z = 1;
                else moving.z = 0;
                return true;
            default:
                break;
            }
        }
        return false;
    }

    Task<> ThirdPersonEntityInputComponent::tick(time_point time, duration delta)
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
            auto new_distance = co_await engine->renderer->raytracer->query(Raytracer::ObjectFlags::LevelCollision, pos, glm::normalize(offset * rot), 0.f, ms * speed);
            entity->setPos(pos + offset * new_distance * rot);
        }
        co_return;
    }
}
