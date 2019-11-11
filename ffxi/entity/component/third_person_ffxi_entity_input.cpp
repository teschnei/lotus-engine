#include "third_person_ffxi_entity_input.h"
#include "engine/core.h"
#include "engine/input.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/component/animation_component.h"
#include <glm/gtx/vector_angle.hpp>

ThirdPersonEntityFFXIInputComponent::ThirdPersonEntityFFXIInputComponent(lotus::Entity* _entity, lotus::Input* _input, lotus::Engine* _engine) : lotus::ThirdPersonEntityInputComponent(_entity, _input), engine(_engine)
{
}

void ThirdPersonEntityFFXIInputComponent::tick(lotus::time_point time, lotus::duration delta)
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
        auto camera_rot = engine->camera->getRot();
        //camera's rot is the direction the boom is (ie. opposite the way the view is facing)
        auto rotated_offset = -offset * camera_rot;
        pos.x += rotated_offset.x;
        pos.z += rotated_offset.z;
        entity->setPos(pos);

        auto entity_quat = entity->getRot();

        glm::vec3 base = glm::normalize(moving);
        auto source_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * entity_quat;
        auto dest_rot_vec = -base * camera_rot;

        auto diff = glm::orientedAngle(glm::normalize(glm::vec2{ source_rot_vec.x, source_rot_vec.z }), glm::normalize(glm::vec2{ dest_rot_vec.x, dest_rot_vec.z }));
        auto max_turn_rate = ms * 0.00001f;
        diff = std::clamp(diff, -max_turn_rate, max_turn_rate);

        auto diff_quat = glm::angleAxis(diff, glm::vec3{ 0.f, 1.f, 0.f });

        auto new_rot = entity_quat * diff_quat;

        entity->setRot(new_rot);
    }

    //play animation
    bool now_moving = moving.x != 0.f || moving.z != 0.f;
    if (!moving_prev && now_moving)
    {
        if (auto animation_component = static_cast<lotus::RenderableEntity*>(entity)->animation_component)
        {
            animation_component->playAnimationLoop("run0");
        }
    }
    else if (moving_prev && !now_moving)
    {
        if (auto animation_component = static_cast<lotus::RenderableEntity*>(entity)->animation_component)
        {
            animation_component->playAnimationLoop("idl0");
        }
    }
    moving_prev = now_moving;
}
