#include "actor_skeleton_static.h"
#include "engine/core.h"
#include "ffxi.h"
#include "dat/sk2.h"
#include "dat/cib.h"
#include "dat/generator.h"
#include "dat/scheduler.h"
#include "dat/mo2.h"
#include <numeric>

namespace FFXI
{
    lotus::Task<std::shared_ptr<const ActorSkeletonStatic>> ActorSkeletonStatic::getSkeleton(lotus::Engine* engine, size_t id)
    {
        auto skel = skeleton_map.find(id);
        if (skel != skeleton_map.end())
        {
            if (auto ptr = skel->second.lock())
            {
                co_return ptr;
            }
        }
        std::array<size_t, 1> ids{ id };
        auto ptr = co_await loadSkeleton(engine, ids);
        skeleton_map.insert_or_assign(id, ptr);
        co_return ptr;
    }

    lotus::Task<std::shared_ptr<const ActorSkeletonStatic>> ActorSkeletonStatic::getSkeleton(lotus::Engine* engine, ActorData::PCDatIDs dat_ids)
    {
        size_t id = dat_ids.skel;
        auto skel = skeleton_map.find(id);
        if (skel != skeleton_map.end())
        {
            if (auto ptr = skel->second.lock())
            {
                co_return ptr;
            }
        }
        std::array<size_t, skeleton_size> skeleton_ids;
        std::iota(skeleton_ids.begin(), skeleton_ids.end(), dat_ids.skel);
        std::array<size_t, battle_animation_size> motion_ids;
        std::iota(motion_ids.begin(), motion_ids.end(), dat_ids.motion);
        std::array<size_t, dw_animation_size> motion_dw_l;
        std::iota(motion_dw_l.begin(), motion_dw_l.end(), dat_ids.motion_dw_l);
        std::array<size_t, dw_animation_size> motion_dw_r;
        std::iota(motion_dw_r.begin(), motion_dw_r.end(), dat_ids.motion_dw_r);
        auto ptr = co_await loadSkeleton(engine, skeleton_ids, motion_ids, motion_dw_l, motion_dw_r);
        skeleton_map.insert_or_assign(id, ptr);
        co_return ptr;
    }

    lotus::WorkerTask<std::shared_ptr<ActorSkeletonStatic>> ActorSkeletonStatic::loadSkeleton(lotus::Engine* engine, std::span<size_t> skeleton_ids, std::span<size_t> motions, std::span<size_t> dw_motions_l, std::span<size_t> dw_motions_r)

    {
        auto skeleton = std::shared_ptr<ActorSkeletonStatic>(new ActorSkeletonStatic());

        for (auto id : skeleton_ids)
        {
            const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(id);
            for (const auto& chunk : dat.root->children)
            {
                if (auto sk2 = dynamic_cast<FFXI::SK2*>(chunk.get()))
                {
                    for (const auto& bone : sk2->bones)
                    {
                        skeleton->static_bone_data.addBone(bone.parent_index, bone.rot, bone.trans);
                    }
                    std::ranges::copy(sk2->generator_points, skeleton->generator_points.begin());
                }
                else if (auto mo2 = dynamic_cast<FFXI::MO2*>(chunk.get()))
                {
                    auto anim = skeleton->animations.find(mo2->name);
                    if (anim == skeleton->animations.end())
                    {
                        anim = skeleton->animations.emplace(mo2->name, std::make_unique<lotus::Animation>(mo2->name, std::chrono::milliseconds(static_cast<int>(1000 * (1.f / 30.f) / mo2->speed)))).first;
                    }
                    auto& animation = anim->second;

                    for (size_t i = 0; i < mo2->frames; ++i)
                    {
                        for (const auto& transform : mo2->animation_data)
                        {
                            auto bone_index = transform.first;
                            auto& mo2_transform = transform.second[i];
                            const auto& bone = skeleton->static_bone_data.bones[bone_index];
                            animation->addFrameData(i, bone_index, bone.parent_bone, bone.rot, bone.trans, { mo2_transform.rot, mo2_transform.trans, mo2_transform.scale });
                        }
                    }
                }
                else if (auto scheduler = dynamic_cast<FFXI::Scheduler*>(chunk.get()))
                {
                    skeleton->scheduler_map.insert({ std::string(chunk->name, 4), scheduler });
                }
                else if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
                {
                    skeleton->generator_map.insert({ std::string(chunk->name, 4), generator });
                }
                else if (auto cib = dynamic_cast<FFXI::Cib*>(chunk.get()))
                {

                }
                else if (chunk->children.size() == 0)
                {
                    DEBUG_BREAK();
                }
            }
        }

        size_t index = 0;
        for (auto id : motions)
        {
            skeleton->battle_animations[index++] = skeleton->loadAnimationDat(engine, id);
        }

        index = 0;
        for (auto id : dw_motions_l)
        {
            skeleton->dw_animations_l[index++] = skeleton->loadAnimationDat(engine, id);
        }

        index = 0;
        for (auto id : dw_motions_r)
        {
            skeleton->dw_animations_r[index++] = skeleton->loadAnimationDat(engine, id);
        }

        co_return skeleton;
    }

    std::unordered_map<std::string, std::unique_ptr<lotus::Animation>> ActorSkeletonStatic::loadAnimationDat(lotus::Engine* engine, size_t dat_id)
    {
        const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_id);
        std::unordered_map<std::string, std::unique_ptr<lotus::Animation>> animation_map;

        for (const auto& chunk : dat.root->children)
        {
            if (auto mo2 = dynamic_cast<FFXI::MO2*>(chunk.get()))
            {
                //TODO: instead of combining, they should be separate so they can be composed of different animations (legs vs torso etc)
                auto anim = animation_map.find(mo2->name);
                if (anim == animation_map.end())
                {
                    anim = animation_map.emplace(mo2->name, std::make_unique<lotus::Animation>(mo2->name, std::chrono::milliseconds(static_cast<int>(1000 * (1.f / 30.f) / mo2->speed)))).first;
                }
                auto& animation = anim->second;

                for (size_t i = 0; i < mo2->frames; ++i)
                {
                    for (const auto& transform : mo2->animation_data)
                    {
                        auto bone_index = transform.first;
                        auto& mo2_transform = transform.second[i];
                        const auto& bone = static_bone_data.bones[bone_index];
                        animation->addFrameData(i, bone_index, bone.parent_bone, bone.rot, bone.trans, {mo2_transform.rot, mo2_transform.trans, mo2_transform.scale});
                    }
                }
            }
            else if (auto scheduler = dynamic_cast<FFXI::Scheduler*>(chunk.get()))
            {
                //TODO
                //scheduler_map.insert({ std::string(chunk->name, 4), scheduler });
            }
            else if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
            {
                DEBUG_BREAK();
            }
        }

        return animation_map;
    }

    ActorSkeletonStatic::ActorSkeletonStatic()
    {

    }

    const std::unordered_map<std::string, FFXI::Scheduler*> ActorSkeletonStatic::getSchedulers() const
    {
        return scheduler_map;
    }

    const std::unordered_map<std::string, FFXI::Generator*> ActorSkeletonStatic::getGenerators() const
    {
        return generator_map;
    }

    const std::unordered_map<std::string, std::unique_ptr<lotus::Animation>>& ActorSkeletonStatic::getAnimations() const
    {
        return animations;
    }

    const std::unordered_map<std::string, std::unique_ptr<lotus::Animation>>& ActorSkeletonStatic::getBattleAnimations(size_t index) const
    {
        return battle_animations[index];
    }

    std::tuple<const std::unordered_map<std::string, std::unique_ptr<lotus::Animation>>&, const std::unordered_map<std::string, std::unique_ptr<lotus::Animation>>&> ActorSkeletonStatic::getBattleAnimationsDualWield(size_t index) const
    {
        return { dw_animations_l[index], dw_animations_r[index] };
    }

    std::span<const FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> ActorSkeletonStatic::getGeneratorPoints() const
    {
        return generator_points;
    }
}