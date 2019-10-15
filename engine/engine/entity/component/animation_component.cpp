#include "animation_component.h"
#include "engine/renderer/skeleton.h"

namespace lotus
{
    AnimationComponent::AnimationComponent(Entity* _entity, std::unique_ptr<Skeleton> _skeleton) : Component(_entity), skeleton(std::move(_skeleton))
    {
        
    }

    void AnimationComponent::tick(time_point time, duration delta)
    {
        //transform skeleton with current animation   
    }
}