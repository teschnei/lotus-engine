#include "classic_third_person_input_component.h"

#include "engine/core.h"
#include "engine/input.h"
#include "engine/renderer/vulkan/renderer.h"
#include <glm/gtx/vector_angle.hpp>

#include <iostream>

namespace FFXI
{
    ClassicThirdPersonInputComponent::ClassicThirdPersonInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, ActorComponent& _actor,
        lotus::Component::AnimationComponent& _animation_component, lotus::Component::CameraComponent& _camera_component) :
        Component(_entity, _engine), actor(_actor), animation_component(_animation_component), camera_component(_camera_component)
    {
    }

    lotus::Task<> ClassicThirdPersonInputComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        auto camera_dir = camera_component.getDir();

        auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());

        if (moving.x != 0 || moving.z != 0)
        {
            auto norm = glm::normalize(glm::vec2{moving.x, moving.z});
            auto moving_quat = glm::angleAxis(glm::orientedAngle(norm, glm::vec2{ 1, 0 }), glm::vec3{ 0, -1, 0 });
            auto camera_quat = glm::angleAxis(glm::orientedAngle(glm::normalize(glm::vec2{camera_dir.x, camera_dir.z}), glm::vec2{1, 0}), glm::vec3{0, -1, 0});
            auto quat = camera_quat * moving_quat;
            float speed = actor.getSpeed() / std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
            glm::vec3 offset = glm::vec3(ms * speed, 0, 0) * quat;
            auto pos = actor.getPos();
            auto width = 0.3f;

            auto new_pos = pos + offset;
            auto step_task = engine->renderer->raytrace_queryer->query(lotus::RaytraceQueryer::ObjectFlags::LevelCollision, pos + step_height, glm::normalize(offset), 0.f, glm::length(offset) + width);
            auto pos_task = engine->renderer->raytrace_queryer->query(lotus::RaytraceQueryer::ObjectFlags::LevelCollision, new_pos + step_height, glm::vec3{ 0.f, 1.f, 0.f }, 0.f, 500.f);

            if (co_await step_task == glm::length(offset) + width)
            {
                actor.setPos(new_pos + step_height + (glm::vec3{ 0.f, 1.f, 0.f } * co_await pos_task));
            }

            actor.setModelOffsetRot(quat);
        }

        //play animation
        bool changed = moving.x != moving_prev.x || moving.z != moving_prev.z;
        if (changed)
        {
            //the movement animations appear to sync at 6 units/s
            float speed = actor.getSpeed() / 6.f;
            if (moving.x != 0 || moving.z != 0)
            {
                animation_component.playAnimationLoop("run", speed);
            }
            else
            {
                animation_component.playAnimationLoop(actor.getIdleAnim());
            }
        }

        moving_prev = moving;
        co_return;
    }

    bool ClassicThirdPersonInputComponent::handleInput(lotus::Input* input, const SDL_Event& event)
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
