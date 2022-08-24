#include "landscape_entity.h"

#include <charconv>

#include "ffxi.h"
#include "ffxi/dat/dat.h"
#include "ffxi/dat/dxt3.h"
#include "ffxi/dat/d3m.h"
#include "ffxi/dat/mmb.h"
#include "ffxi/dat/mzb.h"
#include "ffxi/dat/generator.h"
#include "ffxi/vana_time.h"
#include "entity/component/generator_component.h"
#include "engine/entity/component/static_collision_component.h"
#include "engine/entity/component/instanced_models_component.h"
#include "engine/entity/component/instanced_raster_component.h"
#include "engine/entity/component/instanced_raytrace_component.h"
#include "component/landscape_component.h"
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/color_space.hpp>

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> FFXILandscapeEntity::Init(lotus::Engine* engine, lotus::Scene* scene, size_t zoneid)
{
    auto entity = std::make_shared<lotus::Entity>();
    co_await FFXILandscapeEntity::Load(entity, engine, zoneid, scene);
    co_return std::make_pair(entity, std::tuple<>());
}

lotus::WorkerTask<> FFXILandscapeEntity::Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, size_t zoneid, lotus::Scene* scene)
{
    size_t index = zoneid < 256 ? zoneid + 100 : zoneid + 83635;
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(index);

    FFXI::MZB* mzb{ nullptr };
    std::map<std::string, uint32_t> model_map;
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;
    std::vector<lotus::Task<>> model_tasks;
    std::vector<std::shared_ptr<lotus::Model>> models;
    std::vector<std::shared_ptr<lotus::Model>> water_models;
    std::vector<std::shared_ptr<lotus::Model>> generator_models;

    std::map<std::string, std::map<uint32_t, FFXI::LandscapeComponent::LightTOD>> weather_light_map;
    std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count

    FFXI::DatChunk* model = dat.root.get();
    for (const auto& chunk : dat.root->children)
    {
        if (memcmp(chunk->name, "mode", 4) == 0)
        {
            model = chunk.get();
        }
        else if (memcmp(chunk->name, "weat", 4) == 0)
        {
            auto uint_to_color_vec = [](uint32_t color)
            {
                float r = (color & 0xFF) / 255.f;
                float g = ((color & 0xFF00) >> 8) / 255.f;
                float b = ((color & 0xFF0000) >> 16) / 255.f;
                float a = ((color & 0xFF000000) >> 24) / 128.f;
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
                            //it seems ffxi never did lighting in linear colour space, as these fog colours seem to be the final output values
                            FFXI::LandscapeComponent::LightTOD light =
                            {
                                uint_to_color_vec(casted->data->sunlight_diffuse1) + uint_to_color_vec(casted->data->ambient1),
                                uint_to_color_vec(casted->data->moonlight_diffuse1),
                                uint_to_color_vec(casted->data->ambient1),
                                glm::convertSRGBToLinear(uint_to_color_vec(casted->data->fog1)),
                                casted->data->max_fog_dist1 + casted->data->fog_offset,
                                casted->data->min_fog_dist1 + casted->data->fog_offset,
                                casted->data->brightness1,
                                uint_to_color_vec(casted->data->sunlight_diffuse2) + uint_to_color_vec(casted->data->ambient2),
                                uint_to_color_vec(casted->data->moonlight_diffuse2),
                                uint_to_color_vec(casted->data->ambient2),
                                glm::convertSRGBToLinear(uint_to_color_vec(casted->data->fog2)),
                                casted->data->max_fog_dist2 + casted->data->fog_offset,
                                casted->data->min_fog_dist2 + casted->data->fog_offset,
                                casted->data->brightness2,
                                {},
                                {}
                            };
                            memcpy(light.skybox_altitudes, casted->data->skybox_values, sizeof(float) * 8);
                            for (int i = 0; i < 8; ++i)
                            {
                                light.skybox_colors[i] = glm::convertSRGBToLinear(uint_to_color_vec(casted->data->skybox_colors[i]));
                            }
                            weather_light_map[weather][time] = std::move(light);
                        }
                    }
                }
            }
        }
        else if (memcmp(chunk->name, "effe", 4) == 0)
        {
            for (const auto& chunk2 : chunk->children)
            { 
                //d3s, d3a, mmb
                //children -> generators, keyframes
                if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk2.get()))
                {
                    if (dxt3->width > 0)
                    {
                        texture_tasks.push_back(lotus::Texture::LoadTexture(dxt3->name, FFXI::DXT3Loader::LoadTexture, engine, dxt3));
                    }
                }
                else if (auto d3m = dynamic_cast<FFXI::D3M*>(chunk2.get()))
                {
                    auto [model, model_task] = lotus::Model::LoadModel(std::string(d3m->name, 4), FFXI::D3MLoader::LoadD3M, engine, d3m);
                    //TODO: if these generator models end up in a separate component, don't need to put them in the models array
                    generator_models.push_back(model);
                    models.push_back(model);
                    if (model_task)
                        model_tasks.push_back(std::move(*model_task));
                }
                else if (auto d3a = dynamic_cast<FFXI::D3A*>(chunk2.get()))
                {
                    auto [model, model_task] = lotus::Model::LoadModel(std::string(d3a->name, 4) + "_d3a", FFXI::D3MLoader::LoadD3A, engine, d3a);
                    generator_models.push_back(model);
                    models.push_back(model);
                    if (model_task)
                        model_tasks.push_back(std::move(*model_task));
                }
                else if (auto mmb = dynamic_cast<FFXI::MMB*>(chunk2.get()))
                {
                    auto [model, model_task] = lotus::Model::LoadModel(std::string(mmb->name, 4), FFXI::MMBLoader::LoadModel, engine, mmb);
                    generator_models.push_back(model);
                    models.push_back(model);
                    if (model_task)
                        model_tasks.push_back(std::move(*model_task));
                }
                if (!chunk2->children.empty())
                {
                    for (const auto& chunk3 : chunk2->children)
                    {
                        if (auto generator = dynamic_cast<FFXI::Generator*>(chunk3.get()))
                        {
                            scene->AddComponents(co_await FFXI::GeneratorComponent::make_component(entity.get(), engine, generator, 0ms, nullptr));
                        }
                    }
                }
            }
        }
    }

    for (const auto& chunk : model->children)
    {
        if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                texture_tasks.push_back(lotus::Texture::LoadTexture(dxt3->name, FFXI::DXT3Loader::LoadTexture, engine, dxt3));
            }
        }
        else if (auto mzb_chunk = dynamic_cast<FFXI::MZB*>(chunk.get()))
        {
            mzb = mzb_chunk;
        }
    }

    for (const auto& chunk : model->children)
    {
        if (auto mmb = dynamic_cast<FFXI::MMB*>(chunk.get()))
        {
            std::string name(mmb->name, 16);
            auto [model, model_task] = lotus::Model::LoadModel(name, FFXI::MMBLoader::LoadModel, engine, mmb);
            models.push_back(model);
            if (model_task) model_tasks.push_back(std::move(*model_task));
            model_map[name] = models.size() - 1;
        }
    }

    if (mzb)
    {
        std::map<std::string, std::vector<lotus::Component::InstancedModelsComponent::InstanceInfo>> temp_map;
        std::vector<lotus::Component::InstancedModelsComponent::InstanceInfo> instance_info;

        for (const auto& mzb_piece : mzb->vecMZB)
        {
            std::string name(mzb_piece.id, 16);

            auto pos_mat = glm::translate(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fTransX, mzb_piece.fTransY, mzb_piece.fTransZ });
            auto rot_mat = glm::toMat4(glm::quat{ glm::vec3{mzb_piece.fRotX, mzb_piece.fRotY, mzb_piece.fRotZ} });
            auto scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fScaleX, mzb_piece.fScaleY, mzb_piece.fScaleZ });

            glm::mat4 model = pos_mat * rot_mat * scale_mat;
            glm::mat4 model_t = glm::transpose(model);
            glm::mat3 model_it = glm::transpose(glm::inverse(glm::mat3(model)));
            lotus::Component::InstancedModelsComponent::InstanceInfo info{ model, model_t, model_it };
            temp_map[name].push_back(info);
        }

        for (auto& [name, info_vec] : temp_map)
        {
            instance_offsets[name] = std::make_pair(instance_info.size(), static_cast<uint32_t>(info_vec.size()));
            instance_info.insert(instance_info.end(), std::make_move_iterator(info_vec.begin()), std::make_move_iterator(info_vec.end()));
        }

        //quadtree component?
        //quadtree = *mzb->quadtree;

        if (mzb->water_entries.size() > 0)
        {
            std::unordered_map<float, std::vector<std::pair<glm::vec3, std::pair<glm::vec3, glm::vec3>>>> grouped_water;
            for (const auto& [water_height, entries] : mzb->water_entries)
            {
                auto& height_map = grouped_water[water_height];
                for (const auto& [transform, mesh_entry] : entries)
                {
                    glm::mat4 transform_t = glm::transpose(transform);
                    glm::vec3 pos = transform_t[3];
                    FFXI::CollisionMeshData& mesh = mzb->meshes[mesh_entry];
                    auto bound_min = glm::vec3{ transform_t * glm::vec4{ mesh.bound_min, 1 }};
                    auto bound_max = glm::vec3{ transform_t * glm::vec4{ mesh.bound_max, 1 }};
                    bound_min.y = water_height;
                    bound_max.y = water_height;
                    auto size2 = glm::distance2(bound_min, bound_max);
                    bool found = false;
                    for (auto& [entry_pos, entry_bound] : height_map)
                    {
                        auto& [entry_bound_min, entry_bound_max] = entry_bound;
                        auto entry_size2 = glm::distance2(entry_bound_min, entry_bound_max);
                        if (glm::distance2(entry_pos, pos) / (size2) < 100)
                        {
                            found = true;
                            entry_bound_min = glm::min(entry_bound_min, bound_min);
                            entry_bound_max = glm::max(entry_bound_max, bound_max);
                            break;
                        }
                    }
                    if (!found)
                    {
                        height_map.push_back({ pos, { bound_min, bound_max } });
                    }
                }
            }
            for (const auto& [water_height, entries] : grouped_water)
            {
                for (const auto& [pos, bb] : entries)
                {
                    auto [model, model_task] = lotus::Model::LoadModel("", FFXI::MZB::LoadWaterModel, engine, bb);
                    models.push_back(model);
                    water_models.push_back(model);
                    if (model_task) model_tasks.push_back(std::move(*model_task));
                }
            }
        }
        auto [collision_model, collision_model_task] = lotus::Model::LoadModel("", FFXI::CollisionLoader::LoadModel, engine, mzb->meshes, mzb->mesh_entries);
        std::vector<std::shared_ptr<lotus::Model>> collision_models{ collision_model };

        for (const auto& task : model_tasks)
        {
            co_await task;
        }
        auto models_c = co_await lotus::Component::InstancedModelsComponent::make_component(entity.get(), engine, models, instance_info, instance_offsets);
        auto models_raster = engine->config->renderer.RasterizationEnabled() ? co_await lotus::Component::InstancedRasterComponent::make_component(entity.get(), engine, *models_c) : nullptr;
        auto models_raytrace = engine->config->renderer.RaytraceEnabled() ? co_await lotus::Component::InstancedRaytraceComponent::make_component(entity.get(), engine, *models_c) : nullptr;
        auto coll = co_await lotus::Component::StaticCollisionComponent::make_component(entity.get(), engine, collision_models);
        auto land_comp = co_await FFXI::LandscapeComponent::make_component(entity.get(), engine, std::move(weather_light_map));
        scene->AddComponents(std::move(models_c), std::move(models_raster), std::move(models_raytrace), std::move(coll), std::move(land_comp));

        for (const auto& task : texture_tasks)
        {
            auto texture = co_await task;
        }
        if (collision_model_task) co_await *collision_model_task;
    }
    co_return;
}

//todo: water component
/*
lotus::Task<> FFXILandscapeEntity::tick(lotus::time_point time, lotus::duration delta)
{
    current_time = std::chrono::duration_cast<FFXITime::milliseconds>((FFXITime::vana_time() % FFXITime::days(1))).count() / 60000.f;
    current_time = 860;

    auto elapsed = time - create_time;
    auto frame = (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed) * 7.5f) / std::chrono::milliseconds(1000);

    for (auto& water : water_models)
    {
        water->animation_frame = fmod(frame, 30.f);
    }

    co_return;
}
*/
