#pragma once
#include "engine/entity/component/component.h"
#include "engine/entity/component/deformed_mesh_component.h"
#include "engine/entity/component/deformable_raytrace_component.h"
#include "ffxi/entity/actor_skeleton_static.h"
#include <memory>
#include <unordered_map>
#include <variant>

namespace FFXI
{
    class Scheduler;
    class Generator;
    class Cib;

    class ActorSkeletonComponent : public lotus::Component::Component<ActorSkeletonComponent, lotus::Component::Before<lotus::Component::DeformedMeshComponent, lotus::Component::DeformableRaytraceComponent>>
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

        enum class Slot : uint8_t
        {
            Head = 1,
            Body = 2,
            Hands = 3,
            Legs = 4,
            Feet = 5,
            Weapon = 6,
            WeaponSub = 7,
            WeaponRange = 8
        };

        explicit ActorSkeletonComponent(lotus::Entity*, lotus::Engine* engine, lotus::Component::AnimationComponent& animation_component, lotus::Component::DeformedMeshComponent& deformed_mesh,
            lotus::Component::DeformableRaytraceComponent* raytrace, std::shared_ptr<const ActorSkeletonStatic> skeleton, std::variant<LookData, uint16_t> look,
            std::unordered_map<std::string, FFXI::Scheduler*>&& scheduler_map, std::unordered_map<std::string, FFXI::Generator*>&& generator_map);

        lotus::Component::AnimationComponent& getAnimationComponent() const { return animation_component; }

        std::shared_ptr<const ActorSkeletonStatic> getSkeletonStatic() const { return skeleton; }
        FFXI::Scheduler* getScheduler(std::string name) const;
        FFXI::Generator* getGenerator(std::string name) const;

        void updateEquipLook(uint16_t modelid);
        void updateCib(FFXI::Cib* cib);
        void updateAnimationForCombat(bool entering);

    protected:
        lotus::Component::AnimationComponent& animation_component;
        lotus::Component::DeformedMeshComponent& deformed_component;
        lotus::Component::DeformableRaytraceComponent* raytrace_component;
        std::variant<LookData, uint16_t> look;

        enum class SkeletonType
        {
            None,
            PC,
            NPC,
            MON
        } type;

        std::shared_ptr<const ActorSkeletonStatic> skeleton;
        std::unordered_map<std::string, FFXI::Scheduler*> scheduler_map;
        std::unordered_map<std::string, FFXI::Generator*> generator_map;

        //cib resources
        uint8_t unknown1{ 0xFF }; //from skeleton
        uint8_t footstep1{ 0xFF }; //FootMat?
        uint8_t footstep2{ 0xFF }; //FootSize?
        uint8_t motion_index{ 0xFF };
        uint8_t motion_option{ 0xFF };
        uint8_t weapon_unknown{ 0xFF }; //Shield?
        uint8_t weapon_unknown2{ 0xFF }; //Constrain?
        uint8_t unknown2{ 0xFF }; //probably always empty, maps to XiActorSkeleton's weapon_unknown2 for offhand
        uint8_t weapon_unknown3{ 0xFF }; //never seen this populated, but decompiling says it's from a weapon
        uint8_t body_armour_unknown{ 0xFF }; //Waist?
        uint8_t scale0{ 0xFF };
        uint8_t scale1{ 0xFF };
        uint8_t scale2{ 0xFF };
        uint8_t scale3{ 0xFF };
        uint8_t motion_range_index{ 0xFF };

        uint8_t motion_index_l{ 0xFF };
        uint8_t motion_index_option_l{ 0xFF };

        lotus::Task<> updateEquipLookTask(uint16_t modelid);
    };
}
