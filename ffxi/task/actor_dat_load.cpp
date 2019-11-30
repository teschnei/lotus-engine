#include "actor_dat_load.h"

#include "dat/dat_parser.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/task/renderable_entity_init.h"

ActorDatLoad::ActorDatLoad(const std::shared_ptr<Actor>& _entity, const std::string& _dat) : entity(_entity), dat(_dat)
{
}

void ActorDatLoad::Process(lotus::WorkerThread* thread)
{
    DatParser parser{ dat, thread->engine->renderer.RTXEnabled() };

    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;

    for (const auto& texture_data : parser.getDXT3s())
    {
        if (texture_data->width > 0)
        {
            auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(thread->engine, texture_data->name, texture_data.get());
            texture_map[texture_data->name] = std::move(texture);
        }
    }

    const auto& sk2 = parser.getSK2s()[0];
    auto skel = std::make_unique<lotus::Skeleton>();
    for(const auto& bone : sk2->bones)
    {
        skel->addBone(bone.parent_index, bone.rot, bone.trans);
    }

    for (const auto& mo2 : parser.getMO2s())
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

    entity->addSkeleton(std::move(skel), thread->engine, sizeof(FFXI::OS2::Vertex));

    entity->models.push_back(lotus::Model::LoadModel<FFXIActorLoader>(thread->engine, "iroha_test", &parser.getOS2s(), sk2.get()));

    thread->engine->worker_pool.addWork(std::make_unique<lotus::RenderableEntityInitTask>(entity));
}
