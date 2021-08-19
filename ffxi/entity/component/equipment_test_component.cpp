#include "equipment_test_component.h"

#include "ffxi/entity/actor.h"
#include "engine/input.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"

EquipmentTestComponent::EquipmentTestComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : lotus::InputComponent(_entity, _engine, _input)
{
}

lotus::Task<> EquipmentTestComponent::tick(lotus::time_point time, lotus::duration delta)
{
    if (new_modelid)
    {
        auto* actor = static_cast<Actor*>(entity);
        actor->updateEquipLook(*new_modelid);
        new_modelid.reset();
    }
    co_return;
}

bool EquipmentTestComponent::handleInput(const SDL_Event& event)
{
    if (event.type == SDL_KEYUP && event.key.repeat == 0)
    {
        if (event.key.keysym.scancode == SDL_SCANCODE_G)
        {
            new_modelid = 0x2000 + 65;
            return true;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_H)
        {
            new_modelid = 0x2000;
            return true;
        }
    }
    return false;
}
