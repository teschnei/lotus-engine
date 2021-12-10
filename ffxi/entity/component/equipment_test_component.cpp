#include "equipment_test_component.h"

#include "engine/core.h"
#include "engine/entity/entity.h"
#include "engine/input.h"

#include "ffxi/entity/actor.h"

namespace FFXI
{
    EquipmentTestComponent::EquipmentTestComponent(lotus::Entity* _entity, lotus::Engine* _engine, ActorComponent& _actor, ActorSkeletonComponent& _skeleton) : Component(_entity, _engine), actor(_actor), skeleton(_skeleton)
    {
    }

    lotus::Task<> EquipmentTestComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        if (new_modelid)
        {
            skeleton.updateEquipLook(*new_modelid);
            new_modelid.reset();
        }
        if (btl)
        {
            if (*btl)
            {
                actor.enterCombat();
                btl.reset();
            }
            else
            {
                actor.exitCombat();
                btl.reset();
            }
        }
        co_return;
    }

    bool EquipmentTestComponent::handleInput(lotus::Input* input, const SDL_Event& event)
    {
        if (event.type == SDL_KEYUP && event.key.repeat == 0)
        {
            if (event.key.keysym.scancode == SDL_SCANCODE_G)
            {
                new_modelid = 0x2000 + 64;
                return true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_H)
            {
                new_modelid = 0x2000;
                return true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_J)
            {
                btl = true;
                return true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_K)
            {
                btl = false;
                return true;
            }
        }
        return false;
    }
}
