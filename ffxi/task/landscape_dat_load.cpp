#include "landscape_dat_load.h"

#include <map>
#include <charconv>
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
    FFXI::DatParser parser{dat, thread->engine->renderer.render_mode == lotus::RenderMode::RTX};

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
    for (const auto& chunk : parser.root->children)
    {
        if (memcmp(chunk->name, "weat", 4) == 0)
        {
            auto uint_to_color_vec = [](uint32_t color)
            {
                float r = (color & 0xFF) / 255.f;
                float g = ((color & 0xFF00) >> 8) / 255.f;
                float b = ((color & 0xFF0000) >> 16) / 255.f;
                float a = ((color & 0xFF000000) >> 24) / 255.f;
                return glm::vec4(r, g, b, a);
            };
            for (const auto& chunk2 : chunk->children)
            {
                std::string weather = std::string(chunk2->name, 4);
                for (const auto& chunk3 : chunk2->children)
                {
                    if (auto casted = dynamic_cast<FFXI::Weather*>(chunk3.get()))
                    {
                        uint32_t time = 0;
                        if (auto [p, e] = std::from_chars(casted->name, casted->name + 4, time); e == std::errc())
                        {
                            time = ((time / 100) * 60) + (time - ((time / 100) * 100));
                            FFXILandscapeEntity::LightTOD light =
                            {
                                uint_to_color_vec(casted->data->sunlight_diffuse1),
                                uint_to_color_vec(casted->data->moonlight_diffuse1),
                                uint_to_color_vec(casted->data->ambient1),
                                uint_to_color_vec(casted->data->fog1),
                                casted->data->max_fog_dist1,
                                casted->data->min_fog_dist1,
                                casted->data->brightness1,
                                uint_to_color_vec(casted->data->sunlight_diffuse2),
                                uint_to_color_vec(casted->data->moonlight_diffuse2),
                                uint_to_color_vec(casted->data->ambient2),
                                uint_to_color_vec(casted->data->fog2),
                                casted->data->max_fog_dist2,
                                casted->data->min_fog_dist2,
                                casted->data->brightness2
                            };
                            memcpy(light.skybox_altitudes, casted->data->skybox_values, sizeof(float) * 8);
                            for (int i = 0; i < 8; ++i)
                            {
                                light.skybox_colors[i] = uint_to_color_vec(casted->data->skybox_colors[i]);
                            }
                            entity->weather_light_map[weather][time] = std::move(light);
                        }
                    }
                }
            }
        }
    }
}
