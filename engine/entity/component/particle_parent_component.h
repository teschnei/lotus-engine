#pragma once
#include "component.h"
#include "engine/entity/renderable_entity.h"

namespace lotus
{
    class Particle;
    class ParticleParentComponent : public Component
    {
    public:
        enum class FollowType
        {
            FollowParentPos,
            FollowParentAll
        };

        explicit ParticleParentComponent(Entity*, Engine* engine, std::shared_ptr<RenderableEntity>, FollowType);
        virtual ~ParticleParentComponent() override = default;

        virtual Task<> tick(time_point time, duration delta) override;
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
    protected:
        const FollowType followtype{ FollowType::FollowParentAll };
        const std::shared_ptr<RenderableEntity> parent;
        Particle* const particle{};
    };
}
