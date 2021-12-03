#pragma once
#include "engine/entity/component/component.h"
#include "actor_skeleton_component.h"
#include <optional>

namespace lotus
{
    class Entity;
    class Engine;
}

namespace FFXI
{
    class EquipmentTestComponent : public lotus::Component::Component<EquipmentTestComponent, lotus::Component::Before<ActorSkeletonComponent>>
    {
    public:
        explicit EquipmentTestComponent(lotus::Entity*, lotus::Engine* engine, ActorSkeletonComponent& actor);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
        bool handleInput(lotus::Input*, const SDL_Event&);
    protected:
        ActorSkeletonComponent& actor;
        std::optional<uint16_t> new_modelid;
    };
}
