#pragma once
#include "engine/entity/component/component.h"
#include "actor_pc_models_component.h"
#include <optional>

namespace lotus
{
    class Entity;
    class Engine;
}

namespace FFXI
{
    class EquipmentTestComponent : public lotus::Component::Component<EquipmentTestComponent, ActorPCModelsComponent>
    {
    public:
        explicit EquipmentTestComponent(lotus::Entity*, lotus::Engine* engine, ActorPCModelsComponent& actor);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
        bool handleInput(lotus::Input*, const SDL_Event&);
    protected:
        std::optional<uint16_t> new_modelid;
    };
}
