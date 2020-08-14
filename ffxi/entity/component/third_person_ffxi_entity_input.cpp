#include "third_person_ffxi_entity_input.h"
#include "entity/actor.h"
#include "engine/core.h"
#include "engine/input.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/raytrace_query.h"
#include <glm/gtx/vector_angle.hpp>

ThirdPersonEntityFFXIInputComponent::ThirdPersonEntityFFXIInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : lotus::ThirdPersonEntityInputComponent(_entity, _engine, _input)
{
}

void ThirdPersonEntityFFXIInputComponent::tick(lotus::time_point time, lotus::duration delta)
{
    if (moving.x != 0 || moving.z != 0)
    {
        auto norm = glm::normalize(moving);
        auto camera_rot = engine->camera->getRotX();

        auto rotated_norm = glm::rotateY(-norm, -camera_rot);
        float speed = static_cast<Actor*>(entity)->speed / std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
        auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
        glm::vec3 offset = rotated_norm * glm::vec3(ms * speed);
        auto pos = entity->getPos();
        auto width = 0.3f;

        engine->renderer->raytracer->query(lotus::Raytracer::ObjectFlags::LevelCollision, pos + step_height, glm::normalize(offset), 0.f, glm::length(offset) + width, [this, pos, offset, width](float new_distance)
        {
            if (new_distance == glm::length(offset) + width)
            {
                auto new_pos = pos + (new_distance - width) * glm::normalize(offset);
                engine->renderer->raytracer->query(lotus::Raytracer::ObjectFlags::LevelCollision, new_pos + step_height, glm::vec3{ 0.f, 1.f, 0.f }, 0.f, 500.f, [this, new_pos](float new_distance) {
                    entity->setPos(new_pos + step_height + (glm::vec3{ 0.f, 1.f, 0.f } *new_distance));
                });
            }
        });

        auto entity_quat = entity->getRot();

        glm::vec3 base = glm::normalize(moving);
        auto source_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * entity_quat;
        auto dest_rot_vec = glm::rotateY(-base, -camera_rot);

        auto diff = glm::orientedAngle(glm::normalize(glm::vec2{ source_rot_vec.x, source_rot_vec.z }), glm::normalize(glm::vec2{ dest_rot_vec.x, dest_rot_vec.z }));
        auto max_turn_rate = ms * 0.00001f;
        diff = std::clamp(diff, -max_turn_rate, max_turn_rate);

        auto diff_quat = glm::angleAxis(diff, glm::vec3{ 0.f, 1.f, 0.f });

        auto new_rot = entity_quat * diff_quat;

        entity->setRot(new_rot);
    }

    //play animation
    bool now_moving = moving.x != 0.f || moving.z != 0.f;
    auto deformable = dynamic_cast<lotus::DeformableEntity*>(entity);
    if (!moving_prev && now_moving)
    {
        //the movement animations appear to sync at 6 units/s
        float speed = static_cast<Actor*>(entity)->speed / 6.f;
        if (deformable)
        {
            deformable->animation_component->playAnimationLoop("run0", speed);
        }
    }
    else if (moving_prev && !now_moving)
    {
        if (deformable)
        {
            deformable->animation_component->playAnimationLoop("idl0");
        }
    }
    moving_prev = now_moving;
}
