#include "third_person_ffxiv_entity_input.h"
#include "entity/actor.h"
#include "engine/core.h"
#include "engine/input.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/raytrace_query.h"
#include <glm/gtx/vector_angle.hpp>

ThirdPersonEntityFFXIVInputComponent::ThirdPersonEntityFFXIVInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : lotus::ThirdPersonEntityInputComponent(_entity, _engine, _input)
{
}

lotus::Task<> ThirdPersonEntityFFXIVInputComponent::tick(lotus::time_point time, lotus::duration delta)
{
    auto entity_model_rot = entity->getRot();
    auto entity_game_rot = static_cast<Actor*>(entity)->getGameRot();

    auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());

    if (moving.x != 0 || moving.z != 0)
    {
        auto norm = glm::normalize(moving);
        auto rotated_norm = norm * entity_game_rot;
        float speed = static_cast<Actor*>(entity)->speed / std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
        if (moving.x < 0) speed /= 2;
        glm::vec3 offset = rotated_norm * glm::vec3(ms * speed);
        auto pos = entity->getPos();
        auto width = 0.3f;

        auto new_pos = pos + (glm::length(offset)) * glm::normalize(offset);
        auto step_task = engine->renderer->raytracer->query(lotus::Raytracer::ObjectFlags::LevelCollision, pos + step_height, glm::normalize(offset), 0.f, glm::length(offset) + width);
        auto pos_task = engine->renderer->raytracer->query(lotus::Raytracer::ObjectFlags::LevelCollision, new_pos + step_height, glm::vec3{ 0.f, 1.f, 0.f }, 0.f, 500.f);

        if (co_await step_task == glm::length(offset) + width)
        {
            entity->setPos(new_pos + step_height + (glm::vec3{ 0.f, 1.f, 0.f } * co_await pos_task));
        }
    }

    //play animation
    bool changed = moving.x != moving_prev.x || moving.z != moving_prev.z;
    if (changed)
    {
        //the movement animations appear to sync at 6 units/s
        float speed = static_cast<Actor*>(entity)->speed / 6.f;
        if (moving.x > 0)
        {
            static_cast<Actor*>(entity)->animation_component->playAnimationLoop("run", speed);
        }
        else if (moving.x < 0)
        {
            static_cast<Actor*>(entity)->animation_component->playAnimationLoop("mvb", speed);
        }
        else if (moving.z > 0)
        {
            static_cast<Actor*>(entity)->animation_component->playAnimationLoop("mvl", speed);
        }
        else if (moving.z < 0)
        {
            static_cast<Actor*>(entity)->animation_component->playAnimationLoop("mvr", speed);
        }
        else
        {
            static_cast<Actor*>(entity)->animation_component->playAnimationLoop("idl");
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

    auto source_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * entity_model_rot;
    auto dest_rot_vec = face_dir * entity_game_rot;

    auto diff = glm::orientedAngle(glm::normalize(glm::vec2{ source_rot_vec.x, source_rot_vec.z }), glm::normalize(glm::vec2{ dest_rot_vec.x, dest_rot_vec.z }));
    auto max_turn_rate = ms * 0.00001f;
    diff = std::clamp(diff, -max_turn_rate, max_turn_rate);

    auto diff_quat = glm::angleAxis(diff, glm::vec3{ 0.f, 1.f, 0.f });

    auto new_rot = entity_model_rot * diff_quat;

    entity->setRot(new_rot);

    moving_prev = moving;
    co_return;
}
