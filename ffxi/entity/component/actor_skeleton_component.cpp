#include "actor_skeleton_component.h"

#include "engine/core.h"
#include "ffxi.h"
#include "dat/os2.h"
#include "dat/dxt3.h"
#include "dat/cib.h"
#include "entity/loader/actor_loader.h"

namespace FFXI
{
    ActorSkeletonComponent::ActorSkeletonComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::AnimationComponent& _animation, lotus::Component::DeformedMeshComponent& _deformed,
        lotus::Component::DeformableRaytraceComponent* _raytrace, std::shared_ptr<const ActorSkeletonStatic> _skeleton, std::variant<LookData, uint16_t> _look,
        std::unordered_map<std::string, FFXI::Scheduler*>&& _scheduler_map, std::unordered_map<std::string, FFXI::Generator*>&& _generator_map) :
        Component(_entity, _engine), animation_component(_animation), deformed_component(_deformed), raytrace_component(_raytrace), skeleton(_skeleton),
        look(_look), type(SkeletonType::None), scheduler_map(std::move(_scheduler_map)), generator_map(std::move(_generator_map))
    {
        if (std::holds_alternative<LookData>(look))
            type = SkeletonType::PC;
        else if (std::holds_alternative<uint16_t>(look))
            type = SkeletonType::MON;

        for (const auto& [name, scheduler] : skeleton->getSchedulers())
        {
            scheduler_map.insert_or_assign(name, scheduler);
        }

        for (const auto& [name, generator] : skeleton->getGenerators())
        {
            generator_map.insert_or_assign(name, generator);
        }

        for (const auto& [name, animation] : skeleton->getAnimations())
        {
            animation_component.skeleton->animations.insert_or_assign(name, animation.get());
        }
    }

    FFXI::Scheduler* ActorSkeletonComponent::getScheduler(std::string id) const
    {
        if (auto scheduler = scheduler_map.find(id); scheduler != scheduler_map.end())
            return scheduler->second;
        return nullptr;
    }

    FFXI::Generator* ActorSkeletonComponent::getGenerator(std::string id) const
    {
        if (auto generator = generator_map.find(id); generator != generator_map.end())
            return generator->second;
        return nullptr;
    }

    void ActorSkeletonComponent::updateEquipLook(uint16_t modelid)
    {
        if (type == SkeletonType::PC)
        {
            auto& pclook = std::get<LookData>(look);
            uint8_t slot = modelid >> 12;
            if (slot < 9 && pclook.slots[slot] != modelid)
            {
                pclook.slots[slot] = modelid;
                engine->worker_pool->background(updateEquipLookTask(modelid));
            }
        }
    }

    lotus::Task<> ActorSkeletonComponent::updateEquipLookTask(uint16_t modelid)
    {
        auto& pclook = std::get<LookData>(look);
        const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(modelid, pclook.look.race));

        std::vector<FFXI::OS2*> os2s;
        std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;
        uint8_t slot = modelid >> 12;

        for (const auto& chunk : dat.root->children)
        {
            if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
            {
                if (dxt3->width > 0)
                {
                    texture_tasks.push_back(lotus::Texture::LoadTexture(dxt3->name, FFXI::DXT3Loader::LoadTexture, engine, dxt3));
                }
            }
            else if (auto os2 = dynamic_cast<FFXI::OS2*>(chunk.get()))
            {
                os2s.push_back(os2);
            }
            else if (auto cib = dynamic_cast<FFXI::Cib*>(chunk.get()))
            {

            }
        }

        auto [model, model_task] = lotus::Model::LoadModel(std::string("iroha_test") + std::string(os2s.front()->name, 4) + std::to_string(modelid), FFXIActorLoader::LoadModel, engine, os2s);

        auto init_task = deformed_component.initModel(model);

        for (const auto& task : texture_tasks)
        {
            co_await task;
        }
        if (model_task)
            co_await *model_task;

        auto new_model_transform = co_await std::move(init_task);
        lotus::Component::DeformableRaytraceComponent::ModelAccelerationStructures acceleration;
        if (raytrace_component)
        {
            acceleration = co_await raytrace_component->initModel(new_model_transform);
        }

        co_await engine->worker_pool->waitForFrame();

        deformed_component.replaceModelIndex(std::move(new_model_transform), slot);
        if (raytrace_component)
        {
            raytrace_component->replaceModelIndex(std::move(acceleration), slot);
        }

        co_return;
    }
}
