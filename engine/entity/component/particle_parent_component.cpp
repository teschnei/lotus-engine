#include "particle_parent_component.h"
#include "engine/core.h"
#include "engine/entity/particle.h"

namespace lotus
{
    ParticleParentComponent::ParticleParentComponent(Entity* _entity, Engine* _engine, std::shared_ptr<RenderableEntity> _parent, FollowType _follow) :
        Component(_entity, _engine), followtype(_follow), parent(std::move(_parent)), particle(dynamic_cast<Particle*>(_entity))
    {
    }

    Task<> ParticleParentComponent::tick(time_point time, duration delta)
    {
        if (particle)
        {
            if (followtype == FollowType::FollowParentAll)
                particle->offset_mat = parent->getPosMat() * parent->getRotMat();
            else if (followtype == FollowType::FollowParentPos)
                particle->offset_mat = parent->getPosMat();
        }
        co_return;
    }

    Task<> ParticleParentComponent::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_return;
    }
}
