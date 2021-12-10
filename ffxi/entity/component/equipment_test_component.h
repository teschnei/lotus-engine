#pragma once
#include "engine/entity/component/component.h"
#include "actor_component.h"
#include "actor_skeleton_component.h"
#include <optional>

namespace lotus
{
    class Entity;
    class Engine;
}

namespace FFXI
{
    class EquipmentTestComponent : public lotus::Component::Component<EquipmentTestComponent, lotus::Component::Before<ActorSkeletonComponent, ActorComponent>>
    {
    public:
        explicit EquipmentTestComponent(lotus::Entity*, lotus::Engine* engine, ActorComponent& actor, ActorSkeletonComponent& skeleton);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
        bool handleInput(lotus::Input*, const SDL_Event&);
    protected:
        ActorComponent& actor;
        ActorSkeletonComponent& skeleton;
        std::optional<uint16_t> new_modelid;
        std::optional<bool> btl;
    };
}
