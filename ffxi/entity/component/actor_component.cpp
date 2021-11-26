#include "actor_component.h"

#include "dat/generator.h"
#include "dat/scheduler.h"
#include "engine/core.h"
#include "engine/input.h"
#include <glm/gtx/vector_angle.hpp>

namespace FFXI
{
    ActorComponent::ActorComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::RenderBaseComponent& _base_component,
        lotus::Component::AnimationComponent& _animation,
        std::array<FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax>&& _generator_points,
        std::map<std::string, FFXI::Scheduler*>&& _scheduler_map,
        std::map<std::string, FFXI::Generator*>&& _generator_map) :
        Component(_entity, _engine), base_component(_base_component), animation(_animation), generator_points(std::move(_generator_points)), scheduler_map(std::move(_scheduler_map)), generator_map(std::move(_generator_map))
    {
    }

    lotus::Task<> ActorComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        auto ms = std::min<long long>(1000000, std::chrono::duration_cast<std::chrono::microseconds>(delta).count());

        auto source_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * base_component.getRot();
        auto dest_rot_vec = glm::vec3{ 1.f, 0.f, 0.f } * rot * model_offset_rot;

        auto diff = glm::orientedAngle(glm::normalize(glm::vec2{ source_rot_vec.x, source_rot_vec.z }), glm::normalize(glm::vec2{ dest_rot_vec.x, dest_rot_vec.z }));
        auto max_turn_rate = ms * 0.00001f;
        diff = std::clamp(diff, -max_turn_rate, max_turn_rate);

        auto diff_quat = glm::angleAxis(diff, glm::vec3{ 0.f, 1.f, 0.f });

        auto new_rot = base_component.getRot() * diff_quat;

        base_component.setRot(new_rot);

        base_component.setPos(pos);
        co_return;
    }

    void ActorComponent::setPos(glm::vec3 _pos, bool interpolate)
    {
        pos = _pos;
        if (!interpolate)
        {
            base_component.setPos(_pos);
        }
    }

    void ActorComponent::setRot(glm::quat _rot, bool interpolate)
    {
        rot = _rot;
        if (!interpolate)
        {
            base_component.setRot(_rot * model_offset_rot);
        }
    }

    void ActorComponent::setModelOffsetRot(glm::quat _rot)
    {
        model_offset_rot = _rot;
    }

    std::span<const FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> ActorComponent::getGeneratorPoints() const
    {
        return generator_points;
    }

    FFXI::Scheduler* ActorComponent::getScheduler(std::string name) const
    {
        if (auto s = scheduler_map.find(name); s != scheduler_map.end())
            return s->second;
        return nullptr;
    }

    FFXI::Generator* ActorComponent::getGenerator(std::string name) const
    {
        if (auto g = generator_map.find(name); g != generator_map.end())
            return g->second;
        return nullptr;
    }
}
