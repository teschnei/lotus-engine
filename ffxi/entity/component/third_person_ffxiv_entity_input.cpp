#include "third_person_ffxiv_entity_input.h"
#include "engine/input.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/component/animation_component.h"

ThirdPersonEntityFFXIVInputComponent::ThirdPersonEntityFFXIVInputComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : lotus::ThirdPersonEntityInputComponent(_entity, _engine, _input)
{
}

void ThirdPersonEntityFFXIVInputComponent::tick(lotus::time_point time, lotus::duration delta)
{
    lotus::ThirdPersonEntityInputComponent::tick(time, delta);
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
