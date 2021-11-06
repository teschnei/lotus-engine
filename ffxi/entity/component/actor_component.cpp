#include "actor_component.h"

#include "engine/core.h"
#include "engine/input.h"
#include <glm/gtx/vector_angle.hpp>

namespace FFXI
{
    ActorComponent::ActorComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::PhysicsComponent& physics) :
        Component(_entity, _engine, physics)
    {
        //pos = (glm::vec3(259.f, -87.f, 99.f));
        setPos((glm::vec3(-681.f, -12.f, 161.f)), false);
    }

    lotus::Task<> ActorComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        auto [physics] = dependencies;

        auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());

        auto source_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * physics.getRot();
        auto dest_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * rot * model_offset_rot;

        auto diff = glm::orientedAngle(glm::normalize(glm::vec2{ source_rot_vec.x, source_rot_vec.z }), glm::normalize(glm::vec2{ dest_rot_vec.x, dest_rot_vec.z }));
        auto max_turn_rate = ms * 0.00001f;
        diff = std::clamp(diff, -max_turn_rate, max_turn_rate);

        auto diff_quat = glm::angleAxis(diff, glm::vec3{ 0.f, 1.f, 0.f });

        auto new_rot = physics.getRot() * diff_quat;

        physics.setRot(new_rot);

        physics.setPos(pos);
        co_return;
    }

    void ActorComponent::setPos(glm::vec3 _pos, bool interpolate)
    {
        pos = _pos;
        if (!interpolate)
        {
            auto [physics] = dependencies;
            physics.setPos(_pos);
        }
    }

    void ActorComponent::setRot(glm::quat _rot, bool interpolate)
    {
        rot = _rot;
        if (!interpolate)
        {
            auto [physics] = dependencies;
            physics.setRot(_rot * model_offset_rot);
        }
    }

    void ActorComponent::setModelOffsetRot(glm::quat _rot)
    {
        model_offset_rot = _rot;
    }
}
