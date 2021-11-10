#include "actor_pc_models_component.h"

#include "engine/core.h"
#include "ffxi.h"
#include "dat/os2.h"
#include "dat/dxt3.h"
#include "entity/loader/actor_loader.h"

namespace FFXI
{
    ActorPCModelsComponent::ActorPCModelsComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::DeformedMeshComponent& _deformed,
        lotus::Component::DeformableRaytraceComponent* _raytrace, LookData _look) :
        Component(_entity, _engine), deformed(_deformed), raytrace(_raytrace), look(_look)
    {
        
    }

    lotus::Task<> ActorPCModelsComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        co_return;
    }

    void ActorPCModelsComponent::updateEquipLook(uint16_t modelid)
    {
        uint8_t slot = modelid >> 12;
        if (slot < 9 && look.slots[slot] != modelid)
        {
            look.slots[slot] = modelid;
            engine->worker_pool->background(updateEquipLookTask(modelid));
        }
    }

    lotus::Task<> ActorPCModelsComponent::updateEquipLookTask(uint16_t modelid)
    {
        const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(modelid, look.look.race));

        std::vector<FFXI::OS2*> os2s;
        std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;

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
        }

        auto [model, model_task] = lotus::Model::LoadModel(std::string("iroha_test") + std::string(os2s.front()->name, 4) + std::to_string(modelid), FFXIActorLoader::LoadModel, engine, os2s);

        auto init_task = deformed.initModel(model);

        for (const auto& task : texture_tasks)
        {
            co_await task;
        }
        if (model_task)
            co_await *model_task;

        auto new_model_transform = co_await std::move(init_task);
        lotus::Component::DeformableRaytraceComponent::ModelAccelerationStructures acceleration;
        if (raytrace)
        {
            acceleration = co_await raytrace->initModel(model, new_model_transform);
        }

        co_await engine->worker_pool->waitForFrame();

        uint8_t slot = modelid >> 12;
        deformed.replaceModelIndex(model, std::move(new_model_transform), slot);
        if (raytrace)
        {
            raytrace->replaceModelIndex(std::move(acceleration), slot);
        }

        co_return;
    }
}
