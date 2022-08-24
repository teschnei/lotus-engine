#include "actor.h"
#include "actor_data.h"

#include <ranges>
#include "ffxi.h"
#include "actor_skeleton_static.h"
#include "dat/dat.h"
#include "dat/dat_loader.h"
#include "dat/os2.h"
#include "dat/sk2.h"
#include "dat/dxt3.h"
#include "dat/mo2.h"
#include "dat/cib.h"
#include "entity/loader/actor_loader.h"
#include "engine/renderer/animation.h"

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, Actor::InitComponents>> Actor::Init(lotus::Engine* engine, lotus::Scene* scene, uint16_t modelid)
{
    auto actor = std::make_shared<lotus::Entity>();
    size_t dat_index = 0;
    if (modelid >= 3500)
        dat_index = modelid - 3500 + 101739;
    else if (modelid >= 3000)
        dat_index = modelid - 3000 + 99907;
    else if (modelid >= 1500)
        dat_index = modelid - 1500 + 51795;
    else
        dat_index = modelid + 1300;
    auto skeleton = co_await FFXI::ActorSkeletonStatic::getSkeleton(engine, dat_index);
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_index);

    auto components = co_await Actor::Load(actor, engine, scene, skeleton, modelid, { dat });
    co_return std::make_pair(actor, components);
}

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, Actor::InitPCComponents>> Actor::Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorSkeletonComponent::LookData look)
{
    auto path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
    auto actor = std::make_shared<lotus::Entity>();
    //actor->look = look;
    auto skeleton_dats = ActorData::PCSkeletonIDs[look.look.race - 1];
    auto skeleton = co_await FFXI::ActorSkeletonStatic::getSkeleton(engine, skeleton_dats);

    auto components = co_await Actor::Load(actor, engine, scene, skeleton, look, {
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.face - 1, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.head, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.body, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.hands, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.legs, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.feet, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.weapon, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.weapon_sub, look.look.race)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(Actor::GetPCModelDatID(look.look.weapon_range, look.look.race))
        });
    //auto pc_components = co_await Actor::LoadPC(actor, engine, scene, components, look, {});
    co_return std::make_pair(actor, components);
}

lotus::WorkerTask<Actor::InitComponents> Actor::Load(std::shared_ptr<lotus::Entity> actor, lotus::Engine* engine, lotus::Scene* scene,
    std::shared_ptr<const FFXI::ActorSkeletonStatic> static_skeleton, std::variant<FFXI::ActorSkeletonComponent::LookData, uint16_t> look,
    std::initializer_list<std::reference_wrapper<const FFXI::Dat>> dats)
{
    std::unique_ptr<lotus::Skeleton> skel = std::make_unique<lotus::Skeleton>(static_skeleton->getBoneData());
    std::vector<std::vector<FFXI::OS2*>> os2s;
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;

    std::vector<std::shared_ptr<lotus::Model>> models;
    std::unordered_map<std::string, FFXI::Scheduler*> scheduler_map;
    std::unordered_map<std::string, FFXI::Generator*> generator_map;

    for (const auto& dat : dats)
    {
        std::vector<FFXI::OS2*> dat_os2s;
        for (const auto& chunk : dat.get().root->children)
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
                dat_os2s.push_back(os2);
            }
            else if (auto scheduler = dynamic_cast<FFXI::Scheduler*>(chunk.get()))
            {
                scheduler_map.insert({ std::string(chunk->name, 4), scheduler });
            }
            else if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
            {
                generator_map.insert({ std::string(chunk->name, 4), generator });
            }
            else if (auto cib = dynamic_cast<FFXI::Cib*>(chunk.get()))
            {

            }
        }
        if (!dat_os2s.empty())
        {
            os2s.push_back(std::move(dat_os2s));
        }
    }

    std::vector<lotus::Task<>> model_tasks;

    for (const auto& os2 : os2s)
    {
        auto [model, model_task] = lotus::Model::LoadModel(std::string("iroha_test") + os2.front()->name, FFXIActorLoader::LoadModel, engine, os2);
        models.push_back(model);
        if (model_task)
            model_tasks.push_back(std::move(*model_task));
    }

    //co_await all tasks
    for (const auto& task : texture_tasks)
    {
        co_await task;
    }
    for (const auto& task : model_tasks)
    {
        co_await task;
    }

    auto a = co_await lotus::Component::AnimationComponent::make_component(actor.get(), engine, std::move(skel));
    auto p = co_await lotus::Component::RenderBaseComponent::make_component(actor.get(), engine);
    auto d = co_await lotus::Component::DeformedMeshComponent::make_component(actor.get(), engine, *p, *a, models);
    auto r = engine->config->renderer.RasterizationEnabled() ? co_await lotus::Component::DeformableRasterComponent::make_component(actor.get(), engine, *d, *p) : nullptr;
    auto rt = engine->config->renderer.RaytraceEnabled() ? co_await lotus::Component::DeformableRaytraceComponent::make_component(actor.get(), engine, *d, *p) : nullptr;
    auto ac = co_await FFXI::ActorComponent::make_component(actor.get(), engine, *p, *a);
    auto sc = co_await FFXI::ActorSkeletonComponent::make_component(actor.get(), engine, *a, *d, rt.get(), static_skeleton, look, std::move(scheduler_map), std::move(generator_map));

    a->playAnimation("idl");
    co_return scene->AddComponents(std::move(a), std::move(p), std::move(d), std::move(r), std::move(rt), std::move(ac), std::move(sc));
}

/*
lotus::WorkerTask<std::tuple<FFXI::ActorPCModelsComponent*>> Actor::LoadPC(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene,
    InitComponents components, FFXI::ActorPCModelsComponent::LookData look, std::initializer_list<std::reference_wrapper<const FFXI::Dat>> dats)
{
    auto pc_c = co_await FFXI::ActorPCModelsComponent::make_component(entity.get(), engine, *std::get<lotus::Component::DeformedMeshComponent*>(components),
        *std::get<lotus::Component::AnimationComponent*>(components), std::get<lotus::Component::DeformableRaytraceComponent*>(components), look);
    co_return scene->AddComponents(std::move(pc_c));
}
*/

size_t Actor::GetPCModelDatID(uint16_t modelid, uint8_t race)
{
    uint16_t id = modelid & 0xFFF;
    uint8_t slot = modelid >> 12;

    auto [offset, dat] = *std::prev(ActorData::PCModelIDs[race - 1][slot].upper_bound(id));
    if (dat > 0)
    {
        return dat + (id - offset);
    }
    return 0;
}
