#include "landscape_entity.h"

#include "task/landscape_dat_load.h"
#include "engine/core.h"

std::vector<std::unique_ptr<lotus::WorkItem>> FFXILandscapeEntity::Init(const std::shared_ptr<FFXILandscapeEntity>& sp, const std::filesystem::path& dat)
{
    std::vector<std::unique_ptr<lotus::WorkItem>> ret;
    ret.push_back(std::make_unique<LandscapeDatLoad>(sp, dat));
    return ret;
}

void FFXILandscapeEntity::populate_AS(lotus::TopLevelAccelerationStructure* as, uint32_t image_index)
{
    auto nodes = quadtree.find(engine->camera->frustum);
    for (const auto& node : nodes)
    {
        auto& [model_offset, instance_info] = model_vec[node];
        auto& model = models[model_offset];
        if (!model->meshes.empty() && model->bottom_level_as)
        {
            //glm is column-major so we have to transpose the model matrix for Raytrace
            auto matrix = glm::mat3x4{ instance_info.model_t };
            engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, model->bottom_level_as->resource_index, static_cast<uint32_t>(lotus::Raytracer::ObjectFlags::LevelGeometry), 2);
        }
    }
    for (const auto& collision_model : collision_models)
    {
        //glm is column-major so we have to transpose the model matrix for Raytrace
        auto matrix = glm::mat3x4{1.f};
        engine->renderer->populateAccelerationStructure(as, collision_model->bottom_level_as.get(), matrix, 0, static_cast<uint32_t>(lotus::Raytracer::ObjectFlags::LevelCollision), 0);
    }
}

void FFXILandscapeEntity::render(lotus::Engine* engine, std::shared_ptr<Entity>& sp)
{
    auto& weather_data = weather_light_map[current_weather];
    auto time1 = weather_data.end();
    auto time2 = weather_data.upper_bound(current_time);
    if (time2 != weather_data.begin()) { time1 = time2; time1--; };
    if (time2 == weather_data.end()) time2 = weather_data.begin();

    float a = (float)((current_time - time1->first)) / (time2->first - time1->first);

    auto& light = engine->lights->light;
    if (current_time > 360 && current_time < 1080)
    {
        light.entity.diffuse_color = glm::mix(time1->second.sunlight_diffuse_entity, time2->second.sunlight_diffuse_entity, a);
        light.landscape.diffuse_color = glm::mix(time1->second.sunlight_diffuse_landscape, time2->second.sunlight_diffuse_landscape, a);
    }
    else
    {
        light.entity.diffuse_color = glm::mix(time1->second.moonlight_diffuse_entity, time2->second.moonlight_diffuse_entity, a);
        light.landscape.diffuse_color = glm::mix(time1->second.moonlight_diffuse_landscape, time2->second.moonlight_diffuse_landscape, a);
    }
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

    lotus::LandscapeEntity::render(engine, sp);
}

void FFXILandscapeEntity::tick(lotus::time_point time, lotus::duration delta)
{
}
