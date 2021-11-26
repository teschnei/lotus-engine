#include "modern_third_person_input_component.h"

#include "engine/core.h"
#include "engine/input.h"
#include "engine/renderer/vulkan/renderer.h"
#include <glm/gtx/vector_angle.hpp>

namespace FFXI
{
    ModernThirdPersonInputComponent::ModernThirdPersonInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, ActorComponent& _actor, lotus::Component::AnimationComponent& _animation_component) :
        Component(_entity, _engine), actor(_actor), animation_component(_animation_component)
    {
    }

    lotus::Task<> ModernThirdPersonInputComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        auto game_rot = actor.getRot();

        auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());

        if (moving.x != 0 || moving.z != 0)
        {
            auto norm = glm::normalize(moving);
            auto rotated_norm = norm * game_rot;
            float speed = actor.getSpeed() / std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
            if (moving.x < 0) speed /= 2;
            glm::vec3 offset = rotated_norm * glm::vec3(ms * speed);
            auto pos = actor.getPos();
            auto width = 0.3f;

            auto new_pos = pos + (glm::length(offset)) * glm::normalize(offset);
            auto step_task = engine->renderer->raytrace_queryer->query(lotus::RaytraceQueryer::ObjectFlags::LevelCollision, pos + step_height, glm::normalize(offset), 0.f, glm::length(offset) + width);
            auto pos_task = engine->renderer->raytrace_queryer->query(lotus::RaytraceQueryer::ObjectFlags::LevelCollision, new_pos + step_height, glm::vec3{ 0.f, 1.f, 0.f }, 0.f, 500.f);

            if (co_await step_task == glm::length(offset) + width)
            {
                actor.setPos(new_pos + step_height + (glm::vec3{ 0.f, 1.f, 0.f } * co_await pos_task));
            }
        }

        //play animation
        bool changed = moving.x != moving_prev.x || moving.z != moving_prev.z;
        if (changed)
        {
            //the movement animations appear to sync at 6 units/s
            float speed = actor.getSpeed() / 6.f;
            if (moving.x > 0)
            {
                animation_component.playAnimationLoop("run", speed);
            }
            else if (moving.x < 0)
            {
                animation_component.playAnimationLoop("mvb", speed);
            }
            else if (moving.z > 0)
            {
                animation_component.playAnimationLoop("mvl", speed);
            }
            else if (moving.z < 0)
            {
                animation_component.playAnimationLoop("mvr", speed);
            }
            else
            {
                animation_component.playAnimationLoop("idl");
            }
        }
        auto face_dir = glm::vec3{ 1.f, 0.f, 0.f };
        if (moving.x != 0 && moving.z != 0)
        {
            face_dir = glm::normalize(moving);
            if (moving.x < 0)
            {
                //backwards diagonals face forwards due to backstep animation
                face_dir = -face_dir;
            }
        }

        actor.setModelOffsetRot(glm::angleAxis(glm::orientedAngle(glm::vec2{ 1.f, 0.f }, glm::vec2{ face_dir.x, face_dir.z }), glm::vec3{ 0.f, 1.f, 0.f }));

        moving_prev = moving;
        co_return;
    }

    bool ModernThirdPersonInputComponent::handleInput(lotus::Input* input, const SDL_Event& event)
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
}
