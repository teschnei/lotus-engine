#include "actor_dat_load.h"

#include "dat/dat_parser.h"
#include "dat/dxt3.h"
#include "dat/os2.h"
#include "dat/sk2.h"
#include "dat/mo2.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/task/renderable_entity_init.h"

ActorDatLoad::ActorDatLoad(const std::shared_ptr<Actor>& _entity, const std::filesystem::path& _dat) : entity(_entity), dat(_dat)
{
}

void ActorDatLoad::Process(lotus::WorkerThread* thread)
{
    FFXI::DatParser parser{ dat, thread->engine->renderer.RaytraceEnabled() };

    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;
    auto skel = std::make_unique<lotus::Skeleton>();
    FFXI::SK2* pSk2{ nullptr };
    std::vector<FFXI::OS2*> os2s;

    for (const auto& chunk : parser.root->children)
    {
        if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(thread->engine, dxt3->name, dxt3);
                texture_map[dxt3->name] = std::move(texture);
            }
        }
        else if (auto sk2 = dynamic_cast<FFXI::SK2*>(chunk.get()))
        {
            pSk2 = sk2;
            for(const auto& bone : sk2->bones)
            {
                skel->addBone(bone.parent_index, bone.rot, bone.trans);
            }
        }
        else if (auto mo2 = dynamic_cast<FFXI::MO2*>(chunk.get()))
        {
            std::unique_ptr<lotus::Animation> animation = std::make_unique<lotus::Animation>(skel.get());
            animation->name = mo2->name;
            animation->frame_duration = std::chrono::milliseconds(static_cast<int>(1000 * (1.f / 30.f) / mo2->speed));

            for (size_t i = 0; i < mo2->frames; ++i)
            {
                for (size_t bone = 0; bone < skel->bones.size(); ++bone)
                {
                    if (auto transform = mo2->animation_data.find(bone); transform != mo2->animation_data.end())
                    {
                        auto& mo2_transform = transform->second[i];
                        animation->addFrameData(i, bone, { mo2_transform.rot, mo2_transform.trans, mo2_transform.scale });
                    }
                    else
                    {
                        animation->addFrameData(i, bone, { glm::quat{1, 0, 0, 0}, glm::vec3{0}, glm::vec3{1} });
                    }
                }
            }
            skel->animations[animation->name] = std::move(animation);
        }
        else if (auto os2 = dynamic_cast<FFXI::OS2*>(chunk.get()))
        {
            os2s.push_back(os2);
        }
    }

    entity->addSkeleton(std::move(skel), sizeof(FFXI::OS2::Vertex));

    entity->models.push_back(lotus::Model::LoadModel<FFXIActorLoader>(thread->engine, "iroha_test", os2s, pSk2));

    thread->engine->worker_pool.addWork(std::make_unique<lotus::RenderableEntityInitTask>(entity));
}
