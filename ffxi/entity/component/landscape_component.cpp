#include "landscape_component.h"

#include "engine/core.h"
#include "vana_time.h"
#include <glm/gtx/rotate_vector.hpp>

namespace FFXI
{
    LandscapeComponent::LandscapeComponent(lotus::Entity* _entity, lotus::Engine* _engine, std::map<std::string, std::map<uint32_t, LightTOD>>&& _weather_light_map) :
        Component(_entity, _engine), weather_light_map(std::move(_weather_light_map))
    {
        light_id = engine->lights->AddLight({});
    }

    lotus::Task<> LandscapeComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        current_time = std::chrono::duration_cast<FFXITime::milliseconds>((FFXITime::vana_time() % FFXITime::days(1))).count() / 60000.f;
        current_time = 720;

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
            //glm::vec3 b = glm::rotate(glm::vec3{1000.f, 0.f, 0.f}, rot, glm::vec3{ 0.f, 0.382f, -0.924f });
            glm::vec3 b = glm::rotate(glm::vec3{1000.f, 0.f, 0.f}, rot, glm::vec3{0.f, 0.f, -1.f});
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

        co_return;
    }
}
