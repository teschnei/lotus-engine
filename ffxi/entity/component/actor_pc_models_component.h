#pragma once
#include "engine/entity/component/component.h"
#include "engine/entity/component/deformed_mesh_component.h"
#include "engine/entity/component/deformable_raytrace_component.h"
#include <memory>

namespace FFXI
{
    class ActorPCModelsComponent : public lotus::Component::Component<ActorPCModelsComponent>
    {
    public:
        union LookData
        {
            struct 
            {
                uint8_t race = 1;
                uint8_t face = 1;
                uint16_t head = 0x1000;
                uint16_t body = 0x2000;
                uint16_t hands = 0x3000;
                uint16_t legs = 0x4000;
                uint16_t feet = 0x5000;
                uint16_t weapon = 0x6000;
                uint16_t weapon_sub = 0x7000;
                uint16_t weapon_range = 0x8000;
            } look;
            uint16_t slots[9];
        };

        explicit ActorPCModelsComponent(lotus::Entity*, lotus::Engine* engine, lotus::Component::DeformedMeshComponent& deformed, lotus::Component::DeformableRaytraceComponent* raytrace, LookData look);

        void updateEquipLook(uint16_t modelid);

    protected:
        LookData look;
        lotus::Task<> updateEquipLookTask(uint16_t modelid);
        lotus::Component::DeformedMeshComponent& deformed;
        lotus::Component::DeformableRaytraceComponent* raytrace;
    };
}
