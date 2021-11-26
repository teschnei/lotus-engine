#pragma once
#include "engine/entity/component/component.h"
#include "engine/entity/component/render_base_component.h"
#include "engine/entity/component/animation_component.h"
#include "dat/sk2.h"
#include <memory>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace FFXI
{
    class Scheduler;
    class Generator;
    class ActorComponent : public lotus::Component::Component<ActorComponent, lotus::Component::Before<lotus::Component::RenderBaseComponent>>
    {
    public:
        explicit ActorComponent(lotus::Entity*, lotus::Engine* engine, lotus::Component::RenderBaseComponent& physics,
            lotus::Component::AnimationComponent& animation,
            std::array<FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax>&& generator_points,
            std::map<std::string, FFXI::Scheduler*>&& scheduler_map,
            std::map<std::string, FFXI::Generator*>&& generator_map);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);

        glm::vec3 getPos() const { return pos; }
        glm::quat getRot() const { return rot; }
        float getSpeed() const { return speed; }

        void setPos(glm::vec3 pos, bool interpolate = true);
        void setRot(glm::quat rot, bool interpolate = true);
        void setModelOffsetRot(glm::quat rot);
        void setSpeed(float _speed) { speed = _speed; }

        lotus::Component::AnimationComponent& getAnimationComponent() const { return animation; }
        std::span<const FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> getGeneratorPoints() const;
        FFXI::Scheduler* getScheduler(std::string name) const;
        FFXI::Generator* getGenerator(std::string name) const;

    protected:
        lotus::Component::RenderBaseComponent& base_component;
        glm::vec3 pos{};
        glm::quat rot{ 1.f, 0.f, 0.f, 0.f };
        glm::quat model_offset_rot{ 1.f, 0.f, 0.f, 0.f };
        float speed{ 4.f };
        lotus::Component::AnimationComponent& animation;
        std::array<FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> generator_points{};
        std::map<std::string, FFXI::Scheduler*> scheduler_map{};
        std::map<std::string, FFXI::Generator*> generator_map{};
    };
}
