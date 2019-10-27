#include "landscape_dat_load.h"

#include <map>
#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/task/landscape_entity_init.h"
#include "dat/dat_parser.h"

LandscapeDatLoad::LandscapeDatLoad(const std::shared_ptr<FFXILandscapeEntity>& _entity, const std::string& _dat) : entity(_entity), dat(_dat)
{
}

void LandscapeDatLoad::Process(lotus::WorkerThread* thread)
{
    DatParser parser{dat};

    const auto& mzbs = parser.getMZBs();

    if (mzbs.size() == 1)
    {
        const auto& mzb = mzbs[0];

        std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;

        for (const auto& texture_data : parser.getDXT3s())
        {
            if (texture_data->width > 0)
            {
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(thread->engine, texture_data->name, texture_data.get());
                texture_map[texture_data->name] = std::move(texture);
            }
        }

        for (const auto& mmb : parser.getMMBs())
        {
            std::string name(mmb->name, 16);

            entity->models.push_back(lotus::Model::LoadModel<FFXI::MMBLoader>(thread->engine, name, mmb.get()));
        }

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
        }

        for (auto& [name, info_vec] : temp_map)
        {
            entity->instance_offsets[name] = std::make_pair(instance_info.size(), static_cast<uint32_t>(info_vec.size()));
            instance_info.insert(instance_info.end(), std::make_move_iterator(info_vec.begin()), std::make_move_iterator(info_vec.end()));
        }

        thread->engine->worker_pool.addWork(std::make_unique<lotus::LandscapeEntityInitTask>(entity, std::move(instance_info)));
    }
}
