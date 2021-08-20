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
    if (moving.x != 0 || moving.z != 0)
    {
        auto norm = glm::normalize(moving);
        auto entity_rot = entity->getRot();

        auto rotated_norm = norm * entity_rot;
        float speed = static_cast<Actor*>(entity)->speed / std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
        if (moving.x < 0) speed /= 2;
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
    }

    //play animation
    bool update_anim = moving.x != moving_prev.x || moving.z != moving_prev.z;
    auto deformable = dynamic_cast<lotus::DeformableEntity*>(entity);
    if (deformable)
    {
        if (update_anim)
        {
            //the movement animations appear to sync at 6 units/s
            float speed = static_cast<Actor*>(entity)->speed / 6.f;
            if (moving.x > 0)
            {
                deformable->animation_component->playAnimationLoop("run", speed);
            }
            else if (moving.x < 0)
            {
                deformable->animation_component->playAnimationLoop("mvb", speed);
            }
            else if (moving.z > 0)
            {
                deformable->animation_component->playAnimationLoop("mvl", speed);
            }
            else if (moving.z < 0)
            {
                deformable->animation_component->playAnimationLoop("mvr", speed);
            }
            else
            {
                deformable->animation_component->playAnimationLoop("idl");
            }
        }
    }
    moving_prev = moving;
    co_return;
}
