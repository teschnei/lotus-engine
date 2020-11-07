#include "third_person_ffxiv_entity_input.h"
#include "engine/input.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"

ThirdPersonEntityFFXIVInputComponent::ThirdPersonEntityFFXIVInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : lotus::ThirdPersonEntityInputComponent(_entity, _engine, _input)
{
}

lotus::Task<> ThirdPersonEntityFFXIVInputComponent::tick(lotus::time_point time, lotus::duration delta)
{
    co_await lotus::ThirdPersonEntityInputComponent::tick(time, delta);
    //play animation
    bool now_moving = moving.x != 0.f || moving.z != 0.f;
    auto deformable = dynamic_cast<lotus::DeformableEntity*>(entity);
    if (!moving_prev && now_moving)
    {
        if (deformable)
        {
            deformable->animation_component->playAnimationLoop("run0");
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
