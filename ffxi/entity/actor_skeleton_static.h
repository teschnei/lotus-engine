#pragma once
#include <array>
#include <unordered_map>
#include "actor_data.h"
#include "engine/task.h"
#include "engine/worker_task.h"
#include "engine/renderer/animation.h"
#include "engine/renderer/skeleton.h"
#include "dat/sk2.h"

namespace lotus
{
    class Engine;
}

namespace FFXI
{
    class Scheduler;
    class Generator;
    class MO2;
    class ActorSkeletonStatic
    {
    public:
        static constexpr size_t skeleton_size = 4;
        static constexpr size_t battle_animation_size = 8;
        static constexpr size_t dw_animation_size = 4;
        static lotus::Task<std::shared_ptr<const ActorSkeletonStatic>> getSkeleton(lotus::Engine*, size_t id);
        static lotus::Task<std::shared_ptr<const ActorSkeletonStatic>> getSkeleton(lotus::Engine*, ActorData::PCDatIDs);
        const lotus::Skeleton::BoneData& getBoneData() const { return static_bone_data; }
        const std::unordered_map<std::string, FFXI::Scheduler*> getSchedulers() const;
        const std::unordered_map<std::string, FFXI::Generator*> getGenerators() const;
        //animations unaffected by battle animations (emotes, JA, etc)
        const std::unordered_map<std::string, lotus::Animation>& getGeneralAnimations() const;
        //out of combat animations that update in combat
        const std::unordered_map<std::string, lotus::Animation>& getNonBattleAnimations() const;
        //combat animations 
        const std::unordered_map<std::string, lotus::Animation>& getBattleAnimations(size_t index) const;
        std::tuple<const std::unordered_map<std::string, lotus::Animation>&, const std::unordered_map<std::string, lotus::Animation>&> getBattleAnimationsDualWield(size_t index) const;
        std::span<const FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> getGeneratorPoints() const;
    protected:
        lotus::Skeleton::BoneData static_bone_data;
        std::unordered_map<std::string, FFXI::Scheduler*> scheduler_map;
        std::unordered_map<std::string, FFXI::Generator*> generator_map;
        std::unordered_map<std::string, lotus::Animation> general_animations;
        std::unordered_map<std::string, lotus::Animation> nonbattle_animations;
        std::array<std::unordered_map<std::string, lotus::Animation>, battle_animation_size> battle_animations;
        std::array<std::unordered_map<std::string, lotus::Animation>, dw_animation_size> dw_animations_l;
        std::array<std::unordered_map<std::string, lotus::Animation>, dw_animation_size> dw_animations_r;
        std::array<FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> generator_points{};
        std::unordered_map<std::string, lotus::Animation> loadAnimationDat(lotus::Engine*, size_t id);
        void combineAnimation(lotus::Animation& anim, FFXI::MO2* mo2);
        inline static std::unordered_map<size_t, std::weak_ptr<ActorSkeletonStatic>> skeleton_map;
        static lotus::WorkerTask<std::shared_ptr<ActorSkeletonStatic>> loadSkeleton(lotus::Engine*, std::span<size_t> skeleton_ids, std::span<size_t> motions = {}, std::span<size_t> dw_motions_l = {}, std::span<size_t> dw_motions_r = {});
        ActorSkeletonStatic();
    };
}