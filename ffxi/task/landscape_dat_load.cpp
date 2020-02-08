#include "landscape_dat_load.h"

#include <map>
#include "dat/dat_parser.h"
#include "dat/dxt3.h"
#include "dat/mzb.h"
#include "dat/mmb.h"
#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/task/landscape_entity_init.h"
#include "engine/renderer/acceleration_structure.h"

LandscapeDatLoad::LandscapeDatLoad(const std::shared_ptr<FFXILandscapeEntity>& _entity, const std::string& _dat) : entity(_entity), dat(_dat)
{
}

void LandscapeDatLoad::Process(lotus::WorkerThread* thread)
{
    FFXI::DatParser parser{dat, thread->engine->renderer.RTXEnabled()};


    FFXI::MZB* mzb{ nullptr };
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;
    std::map<std::string, uint32_t> model_map;

    FFXI::DatChunk* model = nullptr;
    for (const auto& chunk : parser.root->children)
    {
        if (memcmp(chunk->name, "mode", 4) == 0)
        {
            model = chunk.get();
            break;
        }
    }

    for (const auto& chunk : model->children)
    {
        if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(thread->engine, dxt3->name, dxt3);
                texture_map[dxt3->name] = std::move(texture);
            }
        }
        else if (auto mzb_chunk = dynamic_cast<FFXI::MZB*>(chunk.get()))
        {
            mzb = mzb_chunk;
        }
        else if (auto mmb = dynamic_cast<FFXI::MMB*>(chunk.get()))
        {
            std::string name(mmb->name, 16);

            entity->models.push_back(lotus::Model::LoadModel<FFXI::MMBLoader>(thread->engine, name, mmb));
            model_map[name] = entity->models.size() - 1;
        }
    }

    if (mzb)
    {
        entity->instance_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(lotus::LandscapeEntity::InstanceInfo) * mzb->vecMZB.size(),
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

        std::map<std::string, std::vector<lotus::LandscapeEntity::InstanceInfo>> temp_map;
        std::vector<lotus::LandscapeEntity::InstanceInfo> instance_info;

        for (const auto& mzb_piece : mzb->vecMZB)
        {
            std::string name(mzb_piece.id, 16);

            auto pos_mat = glm::translate(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fTransX, mzb_piece.fTransY, mzb_piece.fTransZ });
            auto rot_mat = glm::toMat4(glm::quat{ glm::vec3{mzb_piece.fRotX, mzb_piece.fRotY, mzb_piece.fRotZ} });
            auto scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fScaleX, mzb_piece.fScaleY, mzb_piece.fScaleZ });

            glm::mat4 model = pos_mat * rot_mat * scale_mat;
            glm::mat4 model_t = glm::transpose(model);
            glm::mat3 model_it = glm::transpose(glm::inverse(glm::mat3(model)));
            lotus::LandscapeEntity::InstanceInfo info{ model, model_t, model_it };
            temp_map[name].push_back(info);
            entity->model_vec.push_back(std::make_pair(model_map[name], info));
        }

        for (auto& [name, info_vec] : temp_map)
        {
            entity->instance_offsets[name] = std::make_pair(instance_info.size(), static_cast<uint32_t>(info_vec.size()));
            instance_info.insert(instance_info.end(), std::make_move_iterator(info_vec.begin()), std::make_move_iterator(info_vec.end()));
        }

        entity->quadtree = *mzb->quadtree;

        entity->collision_models.push_back(lotus::Model::LoadModel<FFXI::CollisionLoader>(thread->engine, "", std::move(mzb->meshes), std::move(mzb->mesh_entries)));

        thread->engine->worker_pool.addWork(std::make_unique<lotus::LandscapeEntityInitTask>(entity, std::move(instance_info)));
    }
}
