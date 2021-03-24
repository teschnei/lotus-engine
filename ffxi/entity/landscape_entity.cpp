#include "landscape_entity.h"

#include <charconv>

#include "ffxi.h"
#include "ffxi/dat/dat.h"
#include "ffxi/dat/dxt3.h"
#include "ffxi/dat/mmb.h"
#include "ffxi/dat/mzb.h"
#include "ffxi/vana_time.h"
#include <glm/gtx/rotate_vector.hpp>

lotus::Task<std::shared_ptr<FFXILandscapeEntity>> FFXILandscapeEntity::Init(lotus::Engine* engine, const std::filesystem::path& dat)
{
    auto entity = std::make_shared<FFXILandscapeEntity>(engine);
    co_await entity->Load(dat);
    co_return std::move(entity);
}

lotus::WorkerTask<> FFXILandscapeEntity::Load(const std::filesystem::path& path)
{
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(path);

    FFXI::MZB* mzb{ nullptr };
    std::map<std::string, uint32_t> model_map;
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;
    std::vector<lotus::Task<>> model_tasks;

    FFXI::DatChunk* model = nullptr;
    for (const auto& chunk : dat.root->children)
    {
        if (memcmp(chunk->name, "mode", 4) == 0)
        {
            model = chunk.get();
        }
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
                                casted->data->brightness1 * 2.5,
                                uint_to_color_vec(casted->data->sunlight_diffuse2),
                                uint_to_color_vec(casted->data->moonlight_diffuse2),
                                uint_to_color_vec(casted->data->ambient2),
                                uint_to_color_vec(casted->data->fog2),
                                casted->data->max_fog_dist2,
                                casted->data->min_fog_dist2,
                                casted->data->brightness2 * 2.5,
                                {},
                                {}
                            };
                            memcpy(light.skybox_altitudes, casted->data->skybox_values, sizeof(float) * 8);
                            for (int i = 0; i < 8; ++i)
                            {
                                light.skybox_colors[i] = uint_to_color_vec(casted->data->skybox_colors[i]);
                            }
                            weather_light_map[weather][time] = std::move(light);
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
        instance_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(lotus::LandscapeEntity::InstanceInfo) * mzb->vecMZB.size(),
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
            model_vec.push_back(std::make_pair(model_map[name], info));
        }

        for (auto& [name, info_vec] : temp_map)
        {
            instance_offsets[name] = std::make_pair(instance_info.size(), static_cast<uint32_t>(info_vec.size()));
            instance_info.insert(instance_info.end(), std::make_move_iterator(info_vec.begin()), std::make_move_iterator(info_vec.end()));
        }

        quadtree = *mzb->quadtree;

        auto [collision_model, collision_model_task] = lotus::Model::LoadModel("", FFXI::CollisionLoader::LoadModel, engine, mzb->meshes, mzb->mesh_entries);
        collision_models.push_back(collision_model);

        auto init_task = InitWork(std::move(instance_info));

        for (const auto& task : texture_tasks)
        {
            auto texture = co_await task;
        }
        for (const auto& task : model_tasks)
        {
            co_await task;
        }
        if (collision_model_task) co_await *collision_model_task;
        co_await init_task;
        light_id = engine->lights->AddLight({});
    }
}

void FFXILandscapeEntity::populate_AS(lotus::TopLevelAccelerationStructure* as, uint32_t image_index)
{
    //auto nodes = quadtree.find(engine->camera->frustum);
    //for (const auto& node : nodes)
    for(const auto& [model_offset, instance_info] : model_vec)
    {
        //auto& [model_offset, instance_info] = model_vec[node];
        auto& model = models[model_offset];
        if (!model->meshes.empty() && model->bottom_level_as)
        {
            //glm is column-major so we have to transpose the model matrix for Raytrace
            auto matrix = glm::mat3x4{ instance_info.model_t };
            engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, model->resource_index, static_cast<uint32_t>(lotus::Raytracer::ObjectFlags::LevelGeometry), 2);
        }
    }
    for (const auto& collision_model : collision_models)
    {
        //glm is column-major so we have to transpose the model matrix for Raytrace
        auto matrix = glm::mat3x4{1.f};
        engine->renderer->populateAccelerationStructure(as, collision_model->bottom_level_as.get(), matrix, 0, static_cast<uint32_t>(lotus::Raytracer::ObjectFlags::LevelCollision), 0);
    }
}

lotus::Task<> FFXILandscapeEntity::render(lotus::Engine* engine, std::shared_ptr<Entity> sp)
{
    auto& weather_data = weather_light_map[current_weather];
    auto time1 = weather_data.end();
    auto time2 = weather_data.upper_bound(current_time);
    if (time2 != weather_data.begin()) { time1 = time2; time1--; };
    if (time2 == weather_data.end()) time2 = weather_data.begin();

    float a = 1.0;
    if (time1 != time2)
        if (time2->first > time1->first)
            a = (current_time - time1->first) / (time2->first - time1->first);
        else if (current_time > time1->first)
            a = (current_time - time1->first) / ((1440 - time2->first) + time1->first);
        else
            a = ((1440 - time1->first) + current_time) / ((1440 - time2->first) + time1->first);

    auto& light = engine->lights->light;
    light.entity.specular_color = glm::vec4(1.f);
    light.entity.ambient_color = glm::mix(time1->second.ambient_entity, time2->second.ambient_entity, a);
    light.entity.fog_color = glm::mix(time1->second.fog_color_entity, time2->second.fog_color_entity, a);
    light.entity.max_fog = glm::mix(time1->second.max_fog_entity, time2->second.max_fog_entity, a);
    light.entity.min_fog = glm::mix(time1->second.min_fog_entity, time2->second.min_fog_entity, a);
    light.entity.brightness = glm::mix(time1->second.brightness_entity, time2->second.brightness_entity, a);

    light.landscape.specular_color = glm::vec4(1.f);
    light.landscape.ambient_color = glm::mix(time1->second.ambient_landscape, time2->second.ambient_landscape, a);
    light.landscape.fog_color = glm::mix(time1->second.fog_color_landscape, time2->second.fog_color_landscape, a);
    light.landscape.max_fog = glm::mix(time1->second.max_fog_landscape, time2->second.max_fog_landscape, a);
    light.landscape.min_fog = glm::mix(time1->second.min_fog_landscape, time2->second.min_fog_landscape, a);
    light.landscape.brightness = glm::mix(time1->second.brightness_landscape, time2->second.brightness_landscape, a);
    if (current_time > 360 && current_time < 1080)
    {
        glm::vec4 sunlight_colour_entity = glm::mix(time1->second.sunlight_diffuse_entity, time2->second.sunlight_diffuse_entity, a);
        glm::vec4 sunlight_colour_landscape = glm::mix(time1->second.sunlight_diffuse_landscape, time2->second.sunlight_diffuse_landscape, a);
        light.entity.diffuse_color = sunlight_colour_entity;
        light.landscape.diffuse_color = sunlight_colour_landscape;
        //sun radius: 4.7, distance 1000
        auto rot = (current_time / 720 * glm::pi<float>()) - (glm::pi<float>() / 2);
        glm::vec3 b = glm::rotate(glm::vec3{1000.f, 0.f, 0.f}, rot, glm::vec3{ 0.f, 0.382f, -0.924f });
        engine->lights->UpdateLight(light_id, { b, 0.f, sunlight_colour_landscape * light.landscape.brightness, 4.7f });
    }
    else
    {
        glm::vec4 moonlight_colour_entity = glm::mix(time1->second.moonlight_diffuse_entity, time2->second.moonlight_diffuse_entity, a);
        glm::vec4 moonlight_colour_landscape = glm::mix(time1->second.moonlight_diffuse_landscape, time2->second.moonlight_diffuse_landscape, a);
        light.entity.diffuse_color = moonlight_colour_entity;
        light.landscape.diffuse_color = moonlight_colour_landscape;
        //moon radius: 4.5, distance 1000
        auto rot = (current_time / 720 * glm::pi<float>()) - (glm::pi<float>() / 2);
        glm::vec3 b = glm::rotate(glm::vec3{1000.f, 0.f, 0.f}, rot, glm::vec3{ 0.f, 0.382f, -0.924f });
        engine->lights->UpdateLight(light_id, { -b, 0.f, moonlight_colour_landscape * light.landscape.brightness, 4.5f });
    }

    light.skybox_altitudes1 = glm::mix(time1->second.skybox_altitudes[0], time2->second.skybox_altitudes[0], a);
    light.skybox_altitudes2 = glm::mix(time1->second.skybox_altitudes[1], time2->second.skybox_altitudes[1], a);
    light.skybox_altitudes3 = glm::mix(time1->second.skybox_altitudes[2], time2->second.skybox_altitudes[2], a);
    light.skybox_altitudes4 = glm::mix(time1->second.skybox_altitudes[3], time2->second.skybox_altitudes[3], a);
    light.skybox_altitudes5 = glm::mix(time1->second.skybox_altitudes[4], time2->second.skybox_altitudes[4], a);
    light.skybox_altitudes6 = glm::mix(time1->second.skybox_altitudes[5], time2->second.skybox_altitudes[5], a);
    light.skybox_altitudes7 = glm::mix(time1->second.skybox_altitudes[6], time2->second.skybox_altitudes[6], a);
    light.skybox_altitudes8 = glm::mix(time1->second.skybox_altitudes[7], time2->second.skybox_altitudes[7], a);

    for (int i = 0; i < 8; ++i)
    {
        light.skybox_colors[i] = glm::mix(time1->second.skybox_colors[i], time2->second.skybox_colors[i], a);
    }

    co_await lotus::LandscapeEntity::render(engine, sp);
}

lotus::Task<> FFXILandscapeEntity::tick(lotus::time_point time, lotus::duration delta)
{
    current_time = std::chrono::duration_cast<FFXITime::milliseconds>((FFXITime::vana_time() % FFXITime::days(1))).count() / 60000.f;
    current_time = 600;

    co_return;
}
