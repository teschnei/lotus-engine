#include "actor.h"

#include <ranges>
#include "ffxi.h"
#include "dat/dat.h"
#include "dat/dat_loader.h"
#include "dat/os2.h"
#include "dat/sk2.h"
#include "dat/dxt3.h"
#include "dat/mo2.h"
#include "entity/loader/actor_loader.h"
#include "engine/entity/component/animation_component.h"

Actor::Actor(lotus::Engine* engine) : lotus::DeformableEntity(engine)
{
}

lotus::Task<std::shared_ptr<Actor>> Actor::Init(lotus::Engine* engine, size_t modelid)
{
    auto actor = std::make_shared<Actor>(engine);
    actor->model_type = ModelType::NPC;

    size_t dat_index = 0;
    if (modelid >= 3500)
        dat_index = modelid - 3500 + 101739;
    else if (modelid >= 3000)
        dat_index = modelid - 3000 + 99907;
    else if (modelid >= 1500)
        dat_index = modelid - 1500 + 51795;
    else
        dat_index = modelid + 1300;
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_index);

    co_await actor->Load({ dat });
    co_return std::move(actor);
}

lotus::Task<std::shared_ptr<Actor>> Actor::Init(lotus::Engine* engine, LookData look)
{
    auto path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
    auto actor = std::make_shared<Actor>(engine);
    actor->look = look;
    auto dat_ids = GetPCSkeletonDatIDs(look.look.race, look.look.face);

    co_await actor->Load({
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_ids[0]), //skel
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_ids[1]), //skel2
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_ids[2]), //skel3
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_ids[3]), //skel4
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(dat_ids[4]), //face
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(actor->GetPCModelDatID(look.look.head)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(actor->GetPCModelDatID(look.look.body)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(actor->GetPCModelDatID(look.look.hands)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(actor->GetPCModelDatID(look.look.legs)),
        static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(actor->GetPCModelDatID(look.look.feet))
        });
    co_return std::move(actor);
}

lotus::WorkerTask<> Actor::Load(std::initializer_list<std::reference_wrapper<const FFXI::Dat>> dats)
{
    auto skel = std::make_unique<lotus::Skeleton>();
    FFXI::SK2* pSk2{ nullptr };
    std::vector<std::vector<FFXI::OS2*>> os2s;
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;

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
            else if (auto sk2 = dynamic_cast<FFXI::SK2*>(chunk.get()))
            {
                pSk2 = sk2;
                for (const auto& bone : sk2->bones)
                {
                    skel->addBone(bone.parent_index, bone.rot, bone.trans);
                }
                std::ranges::copy(sk2->generator_points, generator_points.begin());
            }
            else if (auto mo2 = dynamic_cast<FFXI::MO2*>(chunk.get()))
            {
                //TODO: instead of combining, they should be separate so they can be composed of different animations (legs vs torso etc)
                auto anim = skel->animations.find(mo2->name);
                if (anim == skel->animations.end())
                {
                    anim= skel->animations.emplace(mo2->name, std::make_unique<lotus::Animation>(skel.get())).first;
                }
                auto& animation = anim->second;
                animation->name = mo2->name;
                animation->frame_duration = std::chrono::milliseconds(static_cast<int>(1000 * (1.f / 30.f) / mo2->speed));

                for (size_t i = 0; i < mo2->frames; ++i)
                {
                    for (const auto& transform : mo2->animation_data)
                    {
                        auto bone = transform.first;
                        auto& mo2_transform = transform.second[i];
                        animation->addFrameData(i, bone, { mo2_transform.rot, mo2_transform.trans, mo2_transform.scale });
                    }
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
        }
        if (!dat_os2s.empty())
        {
            os2s.push_back(std::move(dat_os2s));
        }
    }

    co_await addSkeleton(std::move(skel));
    std::vector<lotus::Task<>> model_tasks;

    for (const auto& os2 : os2s)
    {
        auto [model, model_task] = lotus::Model::LoadModel(std::string("iroha_test") + os2.front()->name, FFXIActorLoader::LoadModel, engine, os2);
        models.push_back(model);
        if (model_task)
            model_tasks.push_back(std::move(*model_task));
    }
    auto init_task = InitWork();

    //co_await all tasks
    for (const auto& task : texture_tasks)
    {
        co_await task;
    }
    for (const auto& task : model_tasks)
    {
        co_await task;
    }
    co_await init_task;
    animation_component->playAnimation("idl");
}

void Actor::updateEquipLook(uint16_t modelid)
{
    uint8_t slot = modelid >> 12;
    if (model_type == ModelType::PC && slot < 9 && look.slots[slot] != modelid)
    {
        look.slots[slot] = modelid;
        engine->worker_pool->background(updateEquipLookTask(modelid));
    }
}

lotus::Task<> Actor::updateEquipLookTask(uint16_t modelid)
{
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(GetPCModelDatID(modelid));

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

    lotus::ModelTransformedGeometry new_model_transform{};
    auto init_task = InitModel(model, new_model_transform);

    for (const auto& task : texture_tasks)
    {
        co_await task;
    }
    if (model_task)
        co_await *model_task;

    co_await init_task;

    co_await engine->worker_pool->waitForFrame();

    uint8_t slot = modelid >> 12;
    std::swap(models[slot], model);
    std::swap(animation_component->transformed_geometries[slot], new_model_transform);

    engine->worker_pool->gpuResource(std::move(model));
    engine->worker_pool->gpuResource(std::move(new_model_transform));

    co_return;
}

std::vector<size_t> Actor::GetPCSkeletonDatIDs(uint8_t race, uint8_t face)
{
    uint16_t face_offset = 0;
    if (race > 5)
    {
        //tarutaru female do not have their own models
        if (race == 6)
            face_offset = 3176;
        --race;
    }

    size_t base = 3896;
    if (race > 5)
    {
        base = 4120;
    }
    std::vector<size_t> dats;
    dats.push_back(base + (race * 3176));
    dats.push_back(base + (race * 3176) + 1);
    dats.push_back(base + (race * 3176) + 2);
    dats.push_back(base + (race * 3176) + 3);
    dats.push_back(base + (race * 3176) + 7 + face + face_offset);
    return dats;
}

size_t Actor::GetPCModelDatID(uint16_t modelid)
{
    uint16_t id = modelid & 0xFFF;
    uint8_t slot = modelid >> 12;
    uint8_t race = look.look.race;
    if (race > 5)
    {
        //tarutaru female do not have their own models
        --race;
    }

    //armour
    if (slot < 6)
    {
        if (id < 256)
        {
            if (race > 5)
                return 3904 + (256 * slot) + (race * 3176) + id;
            else
                return 3680 + (256 * slot) + (race * 3176) + id;
        }
        else if (id < 320)
        {
            return 62811 + (64 * slot) + (race * 448) + (id - 256);
        }
        else if (id < 576)
        {
            return 69455 + (256 * slot) + (race * 1536) + (id - 320);
        }
        else if (id < 608)
        {
            return 98595 + (32 * slot) + (race * 160) + (id - 576);
        }
        else
        {
            return 102577 + (64 * slot) + (race * 320) + (id - 608);
        }
    }
    //weapons
    else
    {
        return 0;
    }
}
